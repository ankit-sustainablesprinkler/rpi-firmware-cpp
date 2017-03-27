#ifndef _MOVING_AVERAGE_H_
#define _MOVING_AVERAGE_H_
#include <vector>
#include <iterator>

template <class T> class MovingAverage
{
public:
	MovingAverage(int size);
	bool addValue(const T &val);
	T getAverage();
private:
	std::vector<T> _values;
	typename std::vector<T>::iterator _current_pos;
	int _size;
	T _sum;
};

#endif //_MOVING_AVERAGE_H_