#pragma once

#include "graphics_api.hpp"
#include <string>
#include <vector>
#include <map>
#include <stack>
#include <utility>

/** Measures time intervals on the GPU when commands have completed
 */
class GPUProfilerGL
{
public:
	GPUProfilerGL() = default;
	~GPUProfilerGL();
	/** Resets the timer for a label
	 * @param name name of label
	 */
	void begin(const std::string &name);
	/** Stops the timer for the last started timer still running.
	 */
	void end();
	/** Returns times for all labels
	 * @return vector of pairs of label, and time in ns 
	 */
	std::vector<std::pair<std::string,uint64_t>> get();
private:
	/// Double-buffered map of label->(timestamp query of beginning, timestamp query of end)
	std::map<std::string, std::pair<GLuint, GLuint>> queries[2];
	/// Stack of last started timers
	std::stack<std::string> names;
	/// Labels by order of call to begin()
	std::vector<std::string> orderedNames[2];
	/// Double buffering flip
	int bufferId = 0;
};