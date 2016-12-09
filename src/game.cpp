#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>

#include "game.h"
#include "opengl.h"
#include "util.h"
#include "concurrent_queue.h"

#include <glm/glm.hpp>
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vector>
#include <deque>
#include <mutex>
#include <atomic>
#include <thread>
#include <functional>
#include <algorithm>

#include "thirdparty/shaun/sweeper.hpp"
#include "thirdparty/shaun/parser.hpp"
#include "thirdparty/lodepng.h"

#include <glm/ext.hpp>

const int MIN_FLARE_DIST = 100.0;
const int MAX_PLANET_DIST = 200.0;

const float CAMERA_FOVY = 40.0;

concurrent_queue<std::pair<std::string,Texture*>> Game::textures_to_load;

Game::Game()
{
	sensitivity = 0.0004;
	light_position = glm::vec3(0,0,0);
	view_speed = glm::vec3(0,0,0);
	max_view_speed = 0.2;
	view_smoothness = 0.85;
	switch_previous_planet = NULL;
	save = false;

	time_warp_values = {1,100,10000,86400,1000000};
	time_warp_index = 0;

	switch_frames = 100;
	switch_frame_current = 0;
	is_switching = false;
	focused_planet_id = 0;
	epoch = 0.0;
	quit = false;

	cameraPolar = glm::vec3(0.0,0.0,8000.0);
	cameraCenter = glm::vec3(0.0,0.0,0.0);
}

Game::~Game()
{
	ring_shader.destroy();
	planet_shader.destroy();
	atmos_shader.destroy();

	ring_obj.destroy();
	planet_obj.destroy();
	skybox_obj.destroy();
	flare_obj.destroy();

	post_processing.destroy();

	glfwTerminate();

	quit = true;
	for (auto it=tl_threads.begin();it != tl_threads.end();++it)
	{
		it->join();
	}
	screenshot_thread->join();
	delete [] screenshot_buffer;
	delete screenshot_thread;
}

void tlThread(std::atomic<bool> &quit, concurrent_queue<std::pair<std::string,Texture*>> &texs, concurrent_queue<TexMipmapData> &tmd, DDSLoader &loader)
{
	std::pair<std::string,Texture*> st;
	while (!quit)
	{
		if (texs.try_next(st))
		{
			loader.load(st.first, *st.second, tmd);
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
}

void ssThread(std::atomic<bool> &quit, unsigned char *buffer, std::atomic<bool> &save, GLFWwindow *win)
{
	while (!quit)
	{
		if (save)
		{
			time_t t = time(0);
			struct tm *now = localtime(&t);
			std::stringstream filename;
			filename << "./screenshots/screenshot_" << (now->tm_year+1900) << "-" << (now->tm_mon+1) << "-" << (now->tm_mday) << "_" << (now->tm_hour) << "-" << (now->tm_min) << "-" << (now->tm_sec) << ".png";
			int width,height;
			glfwGetWindowSize(win, &width, &height);

			unsigned int error = lodepng::encode(filename.str(), buffer, width,height);
			if(error) std::cout << "Can't save screenshot: " << lodepng_error_text(error) << std::endl;
			save = false;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
}

void Game::loadSettingsFile()
{
	try 
	{
		shaun::parser p;
		shaun::object obj = p.parse(read_file("config/settings.sn").c_str());
		shaun::sweeper swp(&obj);

		shaun::sweeper video(swp("video"));
		auto fs = video("fullscreen");
		fullscreen = (fs.is_null())?true:(bool)fs.value<shaun::boolean>();

		if (!fullscreen)
		{
			win_w = video("res_x").value<shaun::number>();
			win_h = video("res_y").value<shaun::number>();
		}

		shaun::sweeper graphics(swp("graphics"));
		auto max_size = graphics("texture_max_size");
		dds_loader.setMaxSize((max_size.is_null())?0:max_size.value<shaun::number>());

		auto ssaa_fact = graphics("ssaa_factor");
		ssaa_factor = (ssaa_fact.is_null())?1.0:ssaa_fact.value<shaun::number>();

		shaun::sweeper controls(swp("controls"));
		sensitivity = controls("sensitivity").value<shaun::number>();
	} 
	catch (shaun::parse_error e)
	{
		std::cout << e << std::endl;
	}
}

void Game::init()
{

	loadSettingsFile();

	if (!glfwInit())
		exit(-1);

	GLFWmonitor* monitor = glfwGetPrimaryMonitor();
	const GLFWvidmode* mode = glfwGetVideoMode(monitor);
	glfwWindowHint(GLFW_RED_BITS, mode->redBits);
	glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
	glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
	glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);

	if (fullscreen)
		win = glfwCreateWindow(mode->width, mode->height, "Roche", monitor, NULL);
	else
		win = glfwCreateWindow(win_w, win_h, "Roche", NULL, NULL);

	if (!win)
	{
		glfwTerminate();
		exit(-1);
	}

	screenshot_buffer = new unsigned char[mode->width*mode->height*4];

	glfwMakeContextCurrent(win);

	// THREAD INITIALIZATION
	thread_count = 1;
	for (int i=0;i<thread_count;++i)
	{
		tl_threads.emplace_back(tlThread, std::ref(quit), std::ref(textures_to_load),std::ref(textures_to_update), std::ref(dds_loader));
	}
	screenshot_thread = new std::thread(ssThread, std::ref(quit), screenshot_buffer, std::ref(save), win);

	GLenum err = glewInit();
	if (GLEW_OK != err)
	{
		std::cout << "Some shit happened: " << glewGetErrorString(err) << std::endl;
	}

	int width, height;
	glfwGetFramebufferSize(win, &width, &height);
	glViewport(0, 0, width, height);
	glDepthMask(GL_FALSE);
	glEnable(GL_BLEND);
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_FRONT);
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glClampColor(GL_CLAMP_READ_COLOR, GL_FALSE);
	glClampColor(GL_CLAMP_VERTEX_COLOR, GL_FALSE);
	glClampColor(GL_CLAMP_FRAGMENT_COLOR, GL_FALSE);

	no_night.create();
	no_clouds.create();
	unsigned char *black = new unsigned char[4]{0,0,0,255};
	unsigned char *trans = new unsigned char[4]{255,255,255,0};
	TexMipmapData(false, no_night, 0, GL_RGBA, 1,1,GL_UNSIGNED_BYTE, black).updateTexture();
	TexMipmapData(false, no_clouds, 0, GL_RGBA, 1,1,GL_UNSIGNED_BYTE, trans).updateTexture();

	generateModels();
	loadShaders();
	loadSkybox();
	loadPlanetFiles();
	glfwGetCursorPos(win, &pre_mouseposx, &pre_mouseposy);
	ratio = width/(float)height;
	cameraPolar.z = focused_planet->getBody().radius*4;
	post_processing.create(win, ssaa_factor);
	post_processing.addShader(new HDRAction(post_hdr));
	post_processing.addShader(new PostProcessingAction(post_default));
}

void Game::generateModels()
{
	float ring_vert[] = 
	{ 0.0,-1.0,0.0,1.0,-0.0,-1.0,
	 +1.0,-1.0,0.0,1.0,+1.0,-1.0,
	 +1.0,+1.0,0.0,1.0,+1.0,+1.0,
	 -0.0,+1.0,0.0,1.0,-0.0,+1.0,
	};

	int ring_ind[] = {0,1,2,2,3,0,0,3,2,2,1,0};

	ring_obj.create();
	ring_obj.updateVerts(24*4, ring_vert);
	ring_obj.updateIndices(12, ring_ind);

	planet_obj.generateSphere(64,64, 1);
	atmos_obj.generateSphere(64,64, 0);
	skybox_obj.generateSphere(16,16,0);

	float flare_vert[] =
	{ -1.0, -1.0, 0.0,1.0,0.0,0.0,
		+1.0, -1.0, 0.0,1.0,1.0,0.0,
		+1.0, +1.0, 0.0,1.0,1.0,1.0,
		-1.0, +1.0, 0.0,1.0,0.0,1.0
	};

	flare_obj.create();
	flare_obj.updateVerts(24*4, flare_vert);
	flare_obj.updateIndices(12, ring_ind);

	flare_tex.create();
	loadTexture("tex/flare.dds", flare_tex);
}

void Game::loadShaders()
{
	ring_shader.loadFromFile("shaders/ring.vert", "shaders/ring.frag");
	planet_shader.loadFromFile("shaders/planet.vert", "shaders/planet.frag");
	atmos_shader.loadFromFile("shaders/atmos.vert", "shaders/atmos.frag");
	sun_shader.loadFromFile("shaders/planet.vert", "shaders/sun.frag");
	skybox_shader.loadFromFile("shaders/skybox.vert", "shaders/skybox.frag");
	flare_shader.loadFromFile("shaders/flare.vert", "shaders/flare.frag");
	post_default.loadFromFile("shaders/post_fxaa.vert", "shaders/post_fxaa.frag");
	post_hdr.loadFromFile("shaders/post.vert", "shaders/post_hdr.frag");
}

void Game::loadSkybox()
{
	skybox.rot_axis = glm::vec3(1,0,0);
	skybox.rot_angle = 60.0;
	skybox.size = 100;
	skybox.tex.create();
	loadTexture("tex/skybox.dds", skybox.tex);
	skybox.load();
}

void Game::loadPlanetFiles()
{
	try 
	{
		shaun::parser p;
		shaun::object obj = p.parse(read_file("config/planets.sn").c_str());
		shaun::sweeper swp(&obj);

		shaun::sweeper pl(swp("planets"));
		for (unsigned int i=0;i<pl.value<shaun::list>().elements().size();++i)
		{
			planets.emplace_back();
			try
			{
				planets.back().createFromFile(pl[i]);
			}
			catch (std::string str)
			{
				std::cout << str << std::endl;
				exit(-1);
			}
		}
		for (Planet &p : planets)
		{
			p.getOrbit().setParentFromName(planets);
		}

		focused_planet = &planets[focused_planet_id];
	} 
	catch (shaun::parse_error e)
	{
		std::cout << e << std::endl;
	}
}

void Game::loadTexture(const std::string &filename, Texture &tex)
{
	textures_to_load.push(std::pair<std::string,Texture*>(filename, &tex));
}

class PlanetCompareByDist
{
public:
	PlanetCompareByDist(const glm::dvec3 &view_pos)
	{
		this->view_pos = view_pos;
	}
	bool operator()(Planet *p1, Planet *p2)
	{
		return getDist(p1) > getDist(p2);
	}
	float getDist(Planet *p)
	{
		glm::dvec3 temp = p->getPosition() - view_pos;
		return glm::dot(temp,temp);
	}
private:
	glm::dvec3 view_pos;
};

bool Game::isPressedOnce(int key)
{
	if (glfwGetKey(win, key))
	{
		if (keysHeld[key]) return false;
		else return (keysHeld[key] = true);
	}
	else
	{
		return (keysHeld[key] = false);
	}
}

void Game::update(double dt)
{
	for (Planet &p: planets)
	{
		p.getOrbit().reset();
	}
	for (Planet &p: planets)
	{
		p.update(epoch);
	}

	if (isPressedOnce(GLFW_KEY_K))
	{
		if (time_warp_index > 0) time_warp_index--;
	}
	if (isPressedOnce(GLFW_KEY_L))
	{
		if (time_warp_index < (int)time_warp_values.size()-1) time_warp_index++;
	}

	epoch += time_warp_values[time_warp_index]*dt;

	if (isPressedOnce(GLFW_KEY_TAB))
	{
		if (is_switching)
		{
			is_switching = false;
			cameraPolar.z = focused_planet->getBody().radius*4;
		}
		else
		{
			switch_previous_planet = focused_planet;
			focused_planet_id = (focused_planet_id+1)%planets.size();
			focused_planet = &planets[focused_planet_id];
			switch_previous_dist = cameraPolar.z;
			is_switching = true;
		} 
	}

	double posX, posY;
	glm::vec2 move;
	glfwGetCursorPos(win, &posX, &posY);
	move.x = -posX+pre_mouseposx;
	move.y = posY-pre_mouseposy;

	cameraCenter = glm::vec3(0,0,0);

	if (glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_2))
	{
		view_speed.x += move.x*sensitivity;
		view_speed.y += move.y*sensitivity;
		for (int i=0;i<2;++i)
		{
			if (view_speed[i] > max_view_speed) view_speed[i] = max_view_speed;
			if (view_speed[i] < -max_view_speed) view_speed[i] = -max_view_speed;
		}
	}
	else if (glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_3))
	{
		view_speed.z += (move.y*sensitivity);
	}

	cameraPolar.x += view_speed.x;
	cameraPolar.y += view_speed.y;
	cameraPolar.z *= 1.0+view_speed.z;

	view_speed *= view_smoothness;

	if (cameraPolar.y > PI/2 - 0.001)
	{
		cameraPolar.y = PI/2 - 0.001;
		view_speed.y = 0;
	}
	if (cameraPolar.y < -PI/2 + 0.001)
	{
		cameraPolar.y = -PI/2 + 0.001;
		view_speed.y = 0;
	}
	float radius = focused_planet->getBody().radius;
	if (cameraPolar.z < radius) cameraPolar.z = radius;

	pre_mouseposx = posX;
	pre_mouseposy = posY;

	if (isPressedOnce(GLFW_KEY_F12) && !save)
	{
		int width,height;
		glfwGetWindowSize(win, &width, &height);
		glReadPixels(0,0,width,height, GL_RGBA, GL_UNSIGNED_BYTE, screenshot_buffer);
		save = true;
	}
}

void Game::render()
{
	if (tl_threads.empty())
	{
		while (textures_to_load.empty())
		{
			auto st = textures_to_load.front();
			dds_loader.load(st.first, *st.second, textures_to_update);
			textures_to_load.pop();
		}
	}

	TexMipmapData d;
	while (textures_to_update.try_next(d))
	{
		d.updateTexture();
	}

	if (switch_frame_current > switch_frames)
	{
		is_switching = false;
		switch_frame_current = 0;
	}

	if (is_switching)
	{
		float t = switch_frame_current/(float)switch_frames;
		double f = 6*t*t*t*t*t-15*t*t*t*t+10*t*t*t;
		cameraCenter = (focused_planet->getPosition() - switch_previous_planet->getPosition())*f + switch_previous_planet->getPosition();
		float target_dist = focused_planet->getBody().radius*4;
		cameraPolar.z = (target_dist - switch_previous_dist)*f + switch_previous_dist;

		++switch_frame_current;
	}
	else
	{
		cameraCenter = focused_planet->getPosition();
	}

	post_processing.bind();
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	
	cameraPos = glm::dvec3(cos(cameraPolar[0])*cos(cameraPolar[1])*cameraPolar[2], sin(cameraPolar[0])*cos(cameraPolar[1])*cameraPolar[2], sin(cameraPolar[1])*cameraPolar[2]);
	projMat = glm::infinitePerspective((float)(CAMERA_FOVY/180*PI),ratio, 0.01f);
	viewMat = glm::lookAt(glm::vec3(cameraPos), glm::vec3(0,0,0), glm::vec3(0,0,1));

	std::vector<Planet*> renderablePlanets; // Planets to be rendered as flares (too far away)

	// Planet sorting between flares and close meshes
	for (Planet &p : planets)
	{
		if ((projMat*viewMat*glm::vec4(p.getPosition() - cameraCenter, 1.0)).z > 0)
			renderablePlanets.push_back(&p);
	}
	
	skybox.render(projMat, viewMat, skybox_shader, skybox_obj);
	
	std::sort(renderablePlanets.begin(),renderablePlanets.end(), PlanetCompareByDist(cameraPos+cameraCenter));
	for (Planet *p : renderablePlanets)
	{
		renderPlanet(*p);
	}

	post_processing.render();
	glfwSwapBuffers(win);
	glfwPollEvents();
}

bool Game::isRunning()
{
	return !glfwGetKey(win, GLFW_KEY_ESCAPE) && !glfwWindowShouldClose(win);
}

glm::mat4 Game::computeLightMatrix(const glm::vec3 &light_dir,const glm::vec3 &light_up, float planet_size, float ring_outer)
{
	glm::mat4 light_mat;
	glm::vec3 nlight_up = glm::normalize(light_up);
	glm::vec3 nlight_dir = - glm::normalize(light_dir);
	glm::vec3 light_right = glm::normalize(glm::cross(nlight_dir, nlight_up));
	nlight_dir *= ring_outer;
	nlight_up = glm::normalize(glm::cross(nlight_dir, light_right)) * planet_size;
	light_right *= planet_size;
	int i;
	for (i=0;i<3;++i)
	{
		light_mat[i][0] = light_right[i];
		light_mat[i][1] = nlight_up[i];
		light_mat[i][2] = -nlight_dir[i];
	}
	return light_mat;
}

void Game::computeRingMatrix(glm::vec3 toward_view, glm::vec3 rings_up, float size, glm::mat4 &near_mat, glm::mat4 &far_mat)
{
	glm::mat4 near_mat_temp = glm::mat4(1.0);
	glm::mat4 far_mat_temp = glm::mat4(1.0);
	rings_up = glm::normalize(rings_up);
	toward_view = glm::normalize(toward_view);

	glm::vec3 rings_right = glm::normalize(glm::cross(rings_up, toward_view));
	glm::vec3 rings_x = glm::normalize(glm::cross(rings_up, rings_right));
	int i;
	for (i=0;i<3;++i)
	{
		near_mat_temp[0][i] = rings_x[i]*size;
		near_mat_temp[1][i] = rings_right[i]*size;
		near_mat_temp[2][i] = rings_up[i]*size;
		far_mat_temp[0][i] = -rings_x[i]*size;
		far_mat_temp[1][i] = -rings_right[i]*size;
		far_mat_temp[2][i] = -rings_up[i]*size;
	}
	near_mat *= near_mat_temp;
	far_mat *= far_mat_temp;
}

void Game::renderPlanet(Planet &p)
{
	glm::vec3 dist = p.getPosition() - cameraCenter - cameraPos;
	float len_dist = glm::length(dist);
	if (len_dist >= MIN_FLARE_DIST*p.getBody().radius)
	{
		flare_shader.use();
		flare_shader.uniform("ratio", ratio);
		flare_shader.uniform("tex", 0);
		flare_tex.use(0);
		int width,height;
		glfwGetWindowSize(win, &width, &height);
		float pixelsize = 1.0/(float)height;

		glm::vec4 posOnScreen = projMat*viewMat*glm::vec4(p.getPosition() - cameraCenter, 1.0);
		float radius = p.getBody().radius;
		float albedo = p.getBody().albedo;
		glm::dvec3 dist = p.getPosition() - cameraCenter - cameraPos;
		float fdist = glm::length(dist);
		float visual_angle = glm::degrees((float)atan(radius/fdist));
		float max_size = std::min(4.0,((std::max(1.f,(fdist/(MIN_FLARE_DIST*radius)))-1.0)*albedo*radius*0.0000000001)+1.0)*pixelsize*2.0;
		float size_on_screen = std::max(max_size,(visual_angle)/CAMERA_FOVY);
		float alpha = (glm::length(dist) - MIN_FLARE_DIST*radius) / (MAX_PLANET_DIST*radius - MIN_FLARE_DIST*radius);
		glm::vec3 color = (2.0+albedo)*p.getBody().mean_color * ((p.getBody().is_star)?1:(
			glm::dot(
				glm::normalize(dist), 
				glm::normalize(p.getPosition()))*0.5+0.5));
		flare_shader.uniform("size", size_on_screen);
		flare_shader.uniform("color", glm::value_ptr(glm::vec4(color,std::min(1.0f,alpha))));

		glm::vec2 pOS = glm::vec2(posOnScreen/posOnScreen.w);
		flare_shader.uniform("pos", glm::value_ptr(pOS));
		flare_obj.render();
	}
	if (len_dist <= MAX_PLANET_DIST*p.getBody().radius)
	{
		Shader &pshad = p.getBody().is_star?sun_shader:planet_shader;

		// Computing planet matrix
		glm::vec3 render_pos = p.getPosition()-cameraCenter;

		// translation
		glm::mat4 planet_mat = glm::translate(glm::mat4(), render_pos);

		// rotation
		glm::vec3 NORTH = glm::vec3(0,0,1);
		glm::quat q = glm::rotate(glm::quat(), (float)acos(glm::dot(NORTH,p.getBody().rotation_axis)), glm::cross(NORTH,p.getBody().rotation_axis));
		q = glm::rotate(q, p.getBody().rotation_angle, NORTH);
		planet_mat *= mat4_cast(q);
		// scale
		planet_mat = glm::scale(planet_mat, glm::vec3(p.getBody().radius));

		glm::vec3 light_dir = glm::normalize(p.getPosition() - glm::dvec3(light_position));
		glm::mat4 light_mat = computeLightMatrix(light_dir, glm::vec3(0,0,1), p.getBody().radius, p.getRing().outer);

		glm::mat4 far_ring_mat, near_ring_mat;
		far_ring_mat = glm::translate(far_ring_mat, render_pos);
		near_ring_mat = glm::translate(near_ring_mat, render_pos);
		computeRingMatrix(render_pos - glm::vec3(cameraPos), p.getRing().normal, p.getRing().outer, near_ring_mat, far_ring_mat);

		if (p.getRing().has_rings)
		{
			// FAR RING RENDER
			ring_shader.use();
			ring_shader.uniform( "projMat", projMat);
			ring_shader.uniform( "viewMat", viewMat);
			ring_shader.uniform( "modelMat", far_ring_mat);
			ring_shader.uniform( "lightMat", light_mat);
			ring_shader.uniform( "ring_color", p.getRing().color);
			ring_shader.uniform( "tex", 0);
			ring_shader.uniform( "minDist", p.getRing().inner/p.getRing().outer);
			p.getRing().tex.use(0);
			ring_obj.render();
		}

		const Atmosphere &atmos = p.getAtmosphere();

		if (atmos.max_height >= 0)
		{ 
			glm::mat4 atmos_mat = glm::scale(planet_mat, glm::vec3(1.0+atmos.max_height/p.getBody().radius));
			atmos_shader.use();
			atmos_shader.uniform( "projMat", projMat);
			atmos_shader.uniform( "viewMat", viewMat);
			atmos_shader.uniform( "modelMat", atmos_mat);
			atmos_shader.uniform( "view_pos", glm::vec3(cameraPos) - render_pos);
			atmos_shader.uniform( "light_dir", -light_dir);
			atmos_shader.uniform( "planet_radius", p.getBody().radius);
			atmos_shader.uniform( "atmos_height", atmos.max_height);
			atmos_shader.uniform( "scale_height", atmos.scale_height);
			atmos_shader.uniform( "K_R", atmos.K_R);
			atmos_shader.uniform( "K_M", atmos.K_M);
			atmos_shader.uniform( "E", atmos.E);
			atmos_shader.uniform( "C_R", atmos.C_R);
			atmos_shader.uniform( "G_M", atmos.G_M);
			glActiveTexture(GL_TEXTURE4);
			glBindTexture(GL_TEXTURE_2D, atmos.lookup_table);
			atmos_shader.uniform( "lookup", 4);
			atmos_obj.render();
		}

		// PLANET RENDER
		pshad.use();
		pshad.uniform( "projMat", projMat);
		pshad.uniform( "viewMat", viewMat);
		pshad.uniform( "modelMat", planet_mat);
		pshad.uniform( "ring_vec", p.getRing().normal);
		pshad.uniform( "light_dir", light_dir);
		pshad.uniform( "cloud_disp", p.getBody().cloud_disp);
		pshad.uniform( "view_pos", cameraPos);
		pshad.uniform( "ring_inner", p.getRing().inner);
		pshad.uniform( "ring_outer", p.getRing().outer);

		pshad.uniform( "rel_viewpos", glm::vec3(cameraPos)-render_pos);
		pshad.uniform( "planet_radius", p.getBody().radius);
		pshad.uniform( "atmos_height", atmos.max_height);
		pshad.uniform( "scale_height", atmos.scale_height);
		pshad.uniform( "K_R", atmos.K_R);
		pshad.uniform( "K_M", atmos.K_M);
		pshad.uniform( "E", atmos.E);
		pshad.uniform( "C_R", atmos.C_R);
		pshad.uniform( "G_M", atmos.G_M);

		pshad.uniform( "diffuse_tex", 0);
		pshad.uniform( "clouds_tex", 1);
		pshad.uniform( "night_tex", 2);
		pshad.uniform( "ring_tex", 3);
		p.getBody().diffuse_tex.use(0);
		if (p.getBody().has_cloud_tex) p.getBody().cloud_tex.use(1); else no_clouds.use(1);
		if (p.getBody().has_night_tex) p.getBody().night_tex.use(2); else no_night.use(2);
		if (p.getRing().has_rings) p.getRing().tex.use(3); else no_clouds.use(3);
		if (atmos.max_height >= 0)
		{
			glActiveTexture(GL_TEXTURE4);
			glBindTexture(GL_TEXTURE_2D, atmos.lookup_table);
			pshad.uniform( "lookup", 4);
		}
		planet_obj.render();

		if (p.getRing().has_rings)
		{
			// FAR RING RENDER
			ring_shader.use();
			ring_shader.uniform( "projMat", projMat);
			ring_shader.uniform( "viewMat", viewMat);
			ring_shader.uniform( "modelMat", near_ring_mat);
			ring_shader.uniform( "lightMat", light_mat);
			ring_shader.uniform( "ring_color", p.getRing().color);
			ring_shader.uniform( "tex", 0);
			ring_shader.uniform( "minDist", p.getRing().inner/p.getRing().outer);
			p.getRing().tex.use(0);
			ring_obj.render();
		}
		p.load();
	} 
	else
	{
		p.unload();
	}

}