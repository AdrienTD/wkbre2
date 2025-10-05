#include "BreakpointManager.h"

std::unique_ptr<BreakpointManager> BreakpointManager::s_instance;

BreakpointManager& BreakpointManager::instance()
{
	static std::mutex funcMutex;
	auto lock = std::lock_guard<std::mutex>(funcMutex);
	if (!s_instance)
		s_instance.reset(new BreakpointManager);
	return *s_instance;
}

void BreakpointManager::setBreakpoint(int fileIndex, int lineNumber)
{
	auto lock = std::lock_guard<std::mutex>(m_accessMutex);
	m_breakpointSet.insert(std::make_pair(fileIndex, lineNumber));
}

void BreakpointManager::removeBreakpoint(int fileIndex, int lineNumber)
{
	auto lock = std::lock_guard<std::mutex>(m_accessMutex);
	m_breakpointSet.erase(std::make_pair(fileIndex, lineNumber));
}

void BreakpointManager::clearAllBreakpoints()
{
	auto lock = std::lock_guard<std::mutex>(m_accessMutex);
	m_breakpointSet.clear();
}

bool BreakpointManager::isBreakpointSet(int fileIndex, int lineNumber)
{
	auto lock = std::lock_guard<std::mutex>(m_accessMutex);
	return m_breakpointSet.count(std::make_pair(fileIndex, lineNumber));
}

void BreakpointManager::checkAndBreak(int fileIndex, int lineNumber)
{
	auto lock = std::unique_lock<std::mutex>(m_accessMutex);
	if (!m_stepOnNextAction && m_breakpointSet.count(std::make_pair(fileIndex, lineNumber)) == 0)
		return;
	m_pleaseUnbreak = false;
	m_currentBreak = std::make_pair(fileIndex, lineNumber);
	m_breakCondVar.wait(lock, [&]() {return m_pleaseUnbreak; });
	m_currentBreak.reset();
}

std::optional<std::pair<int, int>> BreakpointManager::getCurrentBreakLine()
{
	auto lock = std::unique_lock<std::mutex>(m_accessMutex);
	return m_currentBreak;
}

void BreakpointManager::resumeFromBreak()
{
	auto lock = std::lock_guard<std::mutex>(m_accessMutex);
	m_pleaseUnbreak = true;
	m_stepOnNextAction = false;
	m_breakCondVar.notify_all();
}

void BreakpointManager::stepInto()
{
	auto lock = std::lock_guard<std::mutex>(m_accessMutex);
	m_pleaseUnbreak = true;
	m_stepOnNextAction = true;
	m_breakCondVar.notify_all();
}

