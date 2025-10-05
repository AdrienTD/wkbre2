#pragma once

#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <set>

class BreakpointManager
{
public:
	static BreakpointManager& instance();

	void setBreakpoint(int fileIndex, int lineNumber);
	void removeBreakpoint(int fileIndex, int lineNumber);
	void clearAllBreakpoints();
	bool isBreakpointSet(int fileIndex, int lineNumber);

	void checkAndBreak(int fileIndex, int lineNumber);
	std::optional<std::pair<int, int>> getCurrentBreakLine();
	void resumeFromBreak();
	void stepInto();

private:
	BreakpointManager() = default;
	std::mutex m_accessMutex;
	std::condition_variable m_breakCondVar;
	std::set<std::pair<int, int>> m_breakpointSet;
	std::optional<std::pair<int, int>> m_currentBreak;
	bool m_pleaseUnbreak = false;
	bool m_stepOnNextAction = false;
	
	static std::unique_ptr<BreakpointManager> s_instance;
};