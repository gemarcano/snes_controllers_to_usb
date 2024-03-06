// SPDX-License-Identifier: GPL-2.0-or-later OR LGPL-2.1-or-later
// SPDX-FileCopyrightText: Gabriel Marcano, 2024
/// @file

#ifndef SYSLOG_H_
#define SYSLOG_H_

#include <deque>
#include <string_view>
#include <cstddef>
#include <functional>
#include <format>

#include <sys/time.h>

#include <FreeRTOS.h>
#include <semphr.h>

namespace sctu
{
	/** System log class.
	 *
	 * @tparam max_size The maximum size in bytes of the log in memory.
	 */
	template<size_t max_size>
	class syslog
	{
	public:
		/** Add the given string to the log.
		 *
		 * If a print callback is registered, this function will forward the
		 * string to it as well.
		 *
		 * @param[in] str String to store in log.
		 */
		void push(std::string_view str)
		{
			if (str.size() > max_size)
				return; // FIXME return some kind of error?

			while (logs_.size() && str.size() > space_available_)
			{
				space_available_ += logs_.front().record.size();
				logs_.pop_front();
			}

			space_available_ -= str.size();
			timeval tm;
			gettimeofday(&tm, nullptr);
			logs_.emplace_back(std::string(str), std::move(tm));

			if (callback_)
			{
				callback_(str);
			}
		}

		/** Returns the current number of log lines.
		 *
		 * @returns The number of lines in the log.
		 */
		size_t size() const
		{
			return logs_.size();
		}

		/** Returns the size of the log in bytes.
		 *
		 * @returns The size of the log in bytes.
		 */
		size_t bytes() const
		{
			return max_size - space_available_;
		}

		/** Returns the log at the given position.
		 *
		 * @param[in] index Position of log to get. Must be less than the
		 *  current size().
		 *
		 * @returns The log at the requested position.
		 */
		std::string operator[](size_t index) const
		{
			auto& log = logs_[index];
			return std::format("{}.{:0^6} - {}", log.time.tv_sec, log.time.tv_usec, log.record);
		}

		/** Returns the last log inserted.
		 *
		 * The log must have at least one element.
		 *
		 * @returns The last log inserted.
		 */
		std::string_view back() const
		{
			return logs_.back();
		}

		/** Registers a callback function that is called every time a log entry
		 *  is added to the log.
		 *
		 * @tparam Func Function or functor type of the callback. Must be a
		 *  function that takes a string_view as its final argument. All other
		 *  arguments will be passed every invocation.
		 * @tparam Args Argument types of the function or functor.
		 *
		 * @param[in] func Function or functor to register with the log.
		 * @param[in] args Arguments to the function or functor that are passed
		 *  every time the callback is made.
		 */
		template<class Func, class... Args>
		void register_push_callback(Func&& func, Args&&... args)
		{
			auto wrapper = [func, ... args = std::forward<Args>(args)](std::string_view str)
			{
				func(std::forward<Args>(args)..., str);
			};
			callback_ = wrapper;
		}

	private:
		struct log {
			std::string record;
			timeval time;
		};
		size_t space_available_ = max_size;
		std::deque<log> logs_;
		std::function<void(std::string_view)> callback_;
	};

	/** Wrapper around syslog to make it thread-safe.
	 *
	 * Uses a FreeRTOS mutex to limit access to one task at a time.
	 */
	template<class syslog>
	class safe_syslog
	{
	public:
		safe_syslog()
		{
			mutex_ = xSemaphoreCreateBinaryStatic(&mutex_buffer_);
			xSemaphoreGive(mutex_);
		}

		void push(std::string_view&& str)
		{
			xSemaphoreTake(mutex_, portMAX_DELAY);
			log_.push(str);
			xSemaphoreGive(mutex_);
		}

		size_t size() const
		{
			xSemaphoreTake(mutex_, portMAX_DELAY);
			size_t result = log_.size();
			xSemaphoreGive(mutex_);
			return result;
		}

		size_t bytes() const
		{
			xSemaphoreTake(mutex_, portMAX_DELAY);
			size_t result = log_.bytes_();
			xSemaphoreGive(mutex_);
			return result;
		}

		std::string operator[](size_t index) const
		{
			xSemaphoreTake(mutex_, portMAX_DELAY);
			std::string result = std::string(log_[index]);
			xSemaphoreGive(mutex_);
			return result;
		}

		std::string back() const
		{
			xSemaphoreTake(mutex_, portMAX_DELAY);
			std::string result = log_.back();
			xSemaphoreGive(mutex_);
			return result;
		}

		template<class Func, class... Args>
		void register_push_callback(Func&& func, Args&&... args)
		{
			xSemaphoreTake(mutex_, portMAX_DELAY);
			log_.register_push_callback(std::forward<Func>(func), std::forward<Args>(args)...);
			xSemaphoreGive(mutex_);
		}

	private:
		syslog log_;
		StaticSemaphore_t mutex_buffer_;
		SemaphoreHandle_t mutex_;
	};
}

#endif//SYSLOG_H_
