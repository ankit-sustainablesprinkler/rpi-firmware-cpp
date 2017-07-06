#include "moving_average.h"

template<class T>
MovingAverage<T>::MovingAverage(int size)
{
	this->_values.resize(size);
	this->_current_pos = this->_values.begin();
	this->_sum = T(0);
	this->_size = 0;
}

template<class T>
bool MovingAverage<T>::addValue(const T &value)
{
	*_current_pos = value;
	_sum += *_current_pos;
	_current_pos++;
	if(_current_pos >= _values.end())
	{
		_current_pos = _values.begin();
	}
	if(_size < _values.size()) _size ++;
	else _sum -= *_current_pos;
}

template<class T>
void MovingAverage<T>::reset()
{
	_size = 0;
	_sum = T(0);
	std::fill(_values.begin(), _values.end(), T(0));
}


template<class T>
T MovingAverage<T>::getAverage()
{
	if(_size > 0){
		return _sum / _size;
	} else {
		return _sum;
	}
}

template class MovingAverage<int>;
template class MovingAverage<long>;
template class MovingAverage<float>;
template class MovingAverage<double>;