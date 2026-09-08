/* This file is part of 3hs
 * Copyright (C) 2021-2026 hShop developer team
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef inc_circular_queue_hh
#define inc_circular_queue_hh

#include <3ds.h>
#include <utility>

template <typename T>
class CircularQueue
{
	static constexpr int queue_max_size = 8;
public:
	CircularQueue()
	{
		LightSemaphore_Init(&this->ready_to_remove_sem, 0,              queue_max_size);
		LightSemaphore_Init(&this->spaces_avail_sem,    queue_max_size, queue_max_size);
		RecursiveLock_Init(&this->lock);
		this->begin = this->end = 0;
	}

	/* adds a copy of val into the queue. wait for a space if necessary */
	void enqueue(const T& val)
	{
		LightSemaphore_Acquire(&this->spaces_avail_sem, 1);
		RecursiveLock_Lock(&this->lock);
			this->buf[this->end] = val;
			this->end = (this->end + 1) % queue_max_size;
			LightSemaphore_Release(&this->ready_to_remove_sem, 1);
		RecursiveLock_Unlock(&this->lock);
	}

	bool try_enqueue(const T& val) {
		if (LightSemaphore_TryAcquire(&this->spaces_avail_sem, 1))
			return false;

		RecursiveLock_Lock(&this->lock);
			this->buf[this->end] = val;
			this->end = (this->end + 1) % queue_max_size;
			LightSemaphore_Release(&this->ready_to_remove_sem, 1);
		RecursiveLock_Unlock(&this->lock);
		return true;
	}

	/* waits until a value is available and dequeue it */
	T dequeue()
	{
		LightSemaphore_Acquire(&this->ready_to_remove_sem, 1);
		RecursiveLock_Lock(&this->lock);
			T val = std::move(this->buf[this->begin]);
			this->begin = (this->begin + 1) % queue_max_size;
			LightSemaphore_Release(&this->spaces_avail_sem, 1);
		RecursiveLock_Unlock(&this->lock);
		return val;
	}

	bool empty()
	{
		return this->begin == this->end;
	}

private:
	LightSemaphore ready_to_remove_sem,
	               spaces_avail_sem;
	RecursiveLock lock;
	size_t begin, end;
	T buf[queue_max_size];
};

#endif
