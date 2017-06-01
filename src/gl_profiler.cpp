#include "gl_profiler.hpp"

using namespace std;

void GPUProfilerGL::begin(const string name)
{
	int id = (bufferId+1)%2;
	auto &val = queries[id][name].first;
	if (val == 0)
	{
		glGenQueries(1, &val);
	}
	glQueryCounter(val, GL_TIMESTAMP);
	names.push(name);
	orderedNames[id].push_back(name);
}

void GPUProfilerGL::end()
{
	string name = names.top();
	names.pop();
	int id = (bufferId+1)%2;
	auto &val = queries[id][name].second;
	if (val == 0)
	{
		glCreateQueries(GL_TIMESTAMP, 1, &val);
	}
	glQueryCounter(val, GL_TIMESTAMP);
}

vector<pair<string,uint64_t>> GPUProfilerGL::get()
{
	vector<pair<string,uint64_t>> result;
	auto &m = queries[bufferId];
	for (const string name : orderedNames[bufferId])
	{
		auto val = m.find(name);
		GLuint q1 = val->second.first;
		GLuint q2 = val->second.second;
		if (q1 && q2)
		{
			uint64_t start, end;
			glGetQueryObjectui64v(q1, GL_QUERY_RESULT, &start);
			glGetQueryObjectui64v(q2, GL_QUERY_RESULT, &end);
			result.push_back(make_pair(name, end-start));
		}
		if (q1) glDeleteQueries(1, &q1);
		if (q2) glDeleteQueries(1, &q2);
	}

	m.clear();
	orderedNames[bufferId].clear();
	bufferId = (bufferId+1)%2;
	return result;
}

GPUProfilerGL::~GPUProfilerGL()
{
	for (auto q : queries)
	{
		for (auto m : q)
		{
			auto q1 = m.second.first;
			auto q2 = m.second.second;
			if (q1) glDeleteQueries(1, &q1);
			if (q2) glDeleteQueries(1, &q2);
		}
	}
}