#include "gui.hpp"

#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <memory>

#define STB_RECT_PACK_IMPLEMENTATION
#include "thirdparty/stb_rect_pack.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "thirdparty/stb_truetype.h"

#define STB_IMAGE_IMPLEMENTATION
#include "thirdparty/stb_image.h"

using namespace std;

const int codepointMax = 256;

Gui::Font Gui::loadFont(const string &filename)
{
	Handle h = genHandle();
	_fontInfo[h] = {filename};
	return h;
}

Gui::FontSize Gui::loadFontSize(Font font, float size)
{
	Handle h = genHandle();
	_fontSizes[h] = font;
	_fontInfo[font].fontSizeInfo[h].pixelSize = size;
	return h;
}

Gui::Image Gui::loadImage(const string &filename)
{
	// TODO load image
	return genHandle();
}

void Gui::init()
{
	map<Font, stbtt_fontinfo> infos;
	vector<stbrp_rect> rects;
	vector<unique_ptr<unsigned char[]>> ttfFiles;
	const int margin = 1;
	for (auto &p : _fontInfo)
	{
		FontInfo &finfo = p.second;
		const string filename = finfo.filename;
		// Load file
		ifstream in(filename.c_str(), ios::ate | ios::binary);
		if (!in) throw runtime_error("Can't open font " + filename);
		int ttfFileSize = in.tellg();
		unique_ptr<unsigned char[]> data = unique_ptr<unsigned char[]>(
			new unsigned char[ttfFileSize+1]);
		in.seekg(ios::beg);
		in.read((char*)data.get(), ttfFileSize);
		in.close();
		data[ttfFileSize] = '\0';

		// Init font
		const int fontNum = stbtt_GetNumberOfFonts(data.get());
		if (fontNum < 1) throw runtime_error("No font in file " + filename);

		const int fontOffset = stbtt_GetFontOffsetForIndex(data.get(), 0);

		stbtt_fontinfo info;
		stbtt_InitFont(&info, data.get(), fontOffset);

		// V Metrics
		stbtt_GetFontVMetrics(&info, 
			&finfo.ascent, &finfo.descent, &finfo.lineGap);

		// Find glyphs
		finfo.codepointGlyphs.resize(codepointMax);
		for (int i=0;i<codepointMax;++i)
		{
			const int glyph = stbtt_FindGlyphIndex(&info, i);
			finfo.codepointGlyphs[i] = glyph;
			finfo.allGlyphs.insert(glyph);
		}

		// H metrics for each glyph
		for (int i : finfo.allGlyphs)
		{
			int advanceWidth, leftSideBearing;
			stbtt_GetGlyphHMetrics(&info, i, 
				&advanceWidth, &leftSideBearing);
			finfo.advanceWidth[i] = advanceWidth;
			finfo.leftSideBearing[i] = leftSideBearing;
		}

		// Kerning
		for (int i : finfo.allGlyphs)
		{
			for (int j : finfo.allGlyphs)
			{
				finfo.kernAdvance[make_pair(i,j)] = 
					stbtt_GetGlyphKernAdvance(&info, i,j);
			}
		}

		// codepoint info
		for (auto &p2 : finfo.fontSizeInfo)
		{
			auto &fontSizeInfo = p2.second;
			float scale = stbtt_ScaleForPixelHeight(&info, 
				fontSizeInfo.pixelSize);
			fontSizeInfo.scale = scale;
			for (int i : finfo.allGlyphs)
			{
				auto &glyphInfo = fontSizeInfo.glyphInfo[i];
				stbtt_GetGlyphBitmapBox(&info, i, scale, scale,
					&glyphInfo.x0, &glyphInfo.y0,
					&glyphInfo.x1, &glyphInfo.y1);
				rects.push_back({(int)rects.size(), 
					(stbrp_coord)(glyphInfo.x1-glyphInfo.x0+margin*2), 
					(stbrp_coord)(glyphInfo.y1-glyphInfo.y0+margin*2)});
			}
		}
		infos[p.first] = move(info);
		ttfFiles.push_back(move(data));
	}

	// TODO load images

	// Assign rects
	int width = 128;
	int height = 128;
	bool success = false;
	while (!success)
	{
		stbrp_context context;
		vector<stbrp_node> nodes(width);
		stbrp_init_target(&context, width, height, nodes.data(), nodes.size());
		success = (stbrp_pack_rects(&context, rects.data(), rects.size()) != 0);
		if (!success)
		{
			if (width > height) height *= 2;
			else width *= 2;
		}
	}

	sort(rects.begin(), rects.end(), 
		[](const stbrp_rect &r1, const stbrp_rect &r2){
			return r1.id < r2.id;
		});

	// Rasterize fonts to greyscale image
	vector<uint8_t> greyscale(width*height);

	int rectId = 0;
	for (auto &p : _fontInfo)
	{
		FontInfo &finfo = p.second;
		for (auto &p2 : finfo.fontSizeInfo)
		{
			auto &fontSizeInfo = p2.second;
			for (int i : finfo.allGlyphs)
			{
				auto &glyphInfo = fontSizeInfo.glyphInfo[i];
				auto rect = rects[rectId];
				glyphInfo.x = rect.x+margin;
				glyphInfo.y = rect.y+margin;
				glyphInfo.w = rect.w-margin*2;
				glyphInfo.h = rect.h-margin*2;
				stbtt_MakeGlyphBitmap(&infos[p.first], 
					&greyscale[glyphInfo.y*width+glyphInfo.x],
					glyphInfo.w, glyphInfo.h, width, 
					fontSizeInfo.scale, fontSizeInfo.scale, i);
				rectId += 1;
			}
		}
	}

	// Conversion to RGBA
	vector<uint8_t> rgba(width*height*4);

	for (int i=0;i<width*height;++i)
	{
		int value = greyscale[i];
		for (int j=0;j<4;++j) rgba[i*4+j] = value;
	}

	_atlasWidth = width;
	_atlasHeight = height;
	initGraphics(width, height, move(rgba));
}

void Gui::setText(FontSize fontSize, int posX, int posY, const string &text,
	uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
	_textRenderInfo.push_back(
		{_fontSizes[fontSize], fontSize, posX, posY, text, r, g, b, a});
}

void Gui::setImage(Image img, int posX, int posY, float size)
{

}

void Gui::display(int width, int height)
{
	RenderInfo renderInfo = {};

	for (const auto &text : _textRenderInfo)
	{
		const auto &fontInfo = _fontInfo[text.font];
		const auto &fontSizeInfo = fontInfo.fontSizeInfo.at(text.fontSize);
		const float scale = fontSizeInfo.scale;

		float currentPosX = text.posX;
		float currentPosY = text.posY;

		int previousGlyph = -1;

		for (const char c : text.text)
		{
			const int glyphIndex = fontInfo.codepointGlyphs[c];
			const float advanceWidth = fontInfo.advanceWidth.at(glyphIndex)*scale;
			const float kernAdvance = (previousGlyph==-1)?0:
				fontInfo.kernAdvance.at(make_pair(previousGlyph, glyphIndex))*scale;
			const auto &glyphInfo = fontSizeInfo.glyphInfo.at(glyphIndex);

			currentPosX += kernAdvance;

			const float x0 = (currentPosX+glyphInfo.x0)/(float)width;
			const float x1 = (currentPosX+glyphInfo.x1)/(float)width;
			const float y0 = 1-(currentPosY+glyphInfo.y0)/(float)height;
			const float y1 = 1-(currentPosY+glyphInfo.y1)/(float)height;

			const float u0 = (glyphInfo.x)/(float)_atlasWidth;
			const float u1 = (glyphInfo.x+glyphInfo.w)/(float)_atlasWidth;
			const float v0 = (glyphInfo.y)/(float)_atlasHeight;
			const float v1 = (glyphInfo.y+glyphInfo.h)/(float)_atlasHeight;

			const Vertex v[4] = {
				{x0, y0, u0, v0, text.r, text.g, text.b, text.a},
				{x1, y0, u1, v0, text.r, text.g, text.b, text.a},
				{x0, y1, u0, v1, text.r, text.g, text.b, text.a},
				{x1, y1, u1, v1, text.r, text.g, text.b, text.a}
			};

			for (int i : {0,2,1,2,3,1})
				renderInfo.vertices.push_back(v[i]);

			previousGlyph = glyphIndex;
			currentPosX += advanceWidth;
		}
	}

	displayGraphics(move(renderInfo));
	_textRenderInfo.clear();
}

Gui::Handle Gui::genHandle()
{
	static Handle h = 0;
	return ++h;
}