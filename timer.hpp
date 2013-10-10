#ifndef __WARP_TIMER_HPP
#define __WARP_TIMER_HPP

#include <chrono>

namespace ioremap { namespace warp {

class timer
{
	typedef std::chrono::high_resolution_clock clock;
public:
	timer() : m_last_time(clock::now())
	{
	}

	int64_t elapsed() const
	{
		return std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - m_last_time).count();
	}

	int64_t restart()
	{
		clock::time_point time = clock::now();
		std::swap(m_last_time, time);
		return std::chrono::duration_cast<std::chrono::milliseconds>(m_last_time - time).count();
	}

private:
	clock::time_point m_last_time;
};


}} // namespace ioremap::warp

#endif /* __WARP_TIMER_HPP */
