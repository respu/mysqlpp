/*
Copyright (c) 2014-2015 WASPP (waspp.org at gmail dot com)

Distributed under the Boost Software License, Version 1.0.
http://www.boost.org/LICENSE_1_0.txt
*/

#ifndef MYSQLPP_HPP
#define MYSQLPP_HPP

#include <cstdio>
#include <ctime>

#include <mysql/mysql.h>

#include <stdexcept>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace mysqlpp
{

	class exception : public std::runtime_error
	{
	public:
		exception(const char* file, int line, const std::string& what) : std::runtime_error(what)
		{
			std::ostringstream oss;
			oss << file << ":" << line << " " << what;
			err = oss.str();
		}

		~exception() throw() {}

		const char* what() const throw()
		{
			return err.c_str();
		}

	private:
		std::string err;

	};

	struct datetime : st_mysql_time
	{
		datetime()
		{
			std::time_t time_ = std::time(0);
			std::tm time = *std::localtime(&time_);

			set_time(time);
		}

		datetime(const std::string& str)
		{
			if (str.size() > 19)
			{
				throw exception(__FILE__, __LINE__, "datetime cast failed");
			}

			std::tm time;
			int count = std::sscanf(str.c_str(), "%d-%d-%d %d:%d:%d", &time.tm_year, &time.tm_mon, &time.tm_mday, &time.tm_hour, &time.tm_min, &time.tm_sec);
			if (count != 3 && count != 6)
			{
				throw exception(__FILE__, __LINE__, "datetime cast failed");
			}

			time.tm_year -= 1900;
			time.tm_mon -= 1;
			time.tm_isdst = -1;

			if (std::mktime(&time) == -1)
			{
				throw exception(__FILE__, __LINE__, "datetime cast failed");
			}

			set_time(time);
		}

		void set_time(const std::tm& time)
		{
			year = static_cast<unsigned int>(time.tm_year) + 1900;
			month = static_cast<unsigned int>(time.tm_mon) + 1;
			day = static_cast<unsigned int>(time.tm_mday);
			hour = static_cast<unsigned int>(time.tm_hour);
			minute = static_cast<unsigned int>(time.tm_min);
			second = static_cast<unsigned int>(time.tm_sec);
		}

		std::tm c_tm()
		{
			std::tm time;

			time.tm_year = static_cast<int>(year)-1900;
			time.tm_mon = static_cast<int>(month)-1;
			time.tm_mday = static_cast<int>(day);
			time.tm_hour = static_cast<int>(hour);
			time.tm_min = static_cast<int>(minute);
			time.tm_sec = static_cast<int>(second);
			time.tm_isdst = -1;

			return time;
		}

		std::string str()
		{
			char buf[32] = { 0 };

			std::tm time = c_tm();
			std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &time);

			return std::string(buf);
		}
	};

	struct st_mysql_column
	{
		st_mysql_column() : buffer(0), length(0), is_unsigned(0), is_null(0), error(0)
		{
		}

		enum_field_types type;
		std::string name;

		std::vector<char> buffer;
		unsigned long length;

		char is_unsigned;
		char is_null;
		char error;
	};

	class result
	{
	public:
		result(st_mysql_stmt* stmt_);
		~result();

		bool bind();
		unsigned long long int num_rows();
		bool fetch(bool is_proc = false);

		template<typename T>
		T get(unsigned int index)
		{
			st_mysql_column& column = get_column(index);
			if (column.is_null)
			{
				throw exception(__FILE__, __LINE__, "null value field");
			}

			T value = T();
			fetch_column(column, value);

			return value;
		}

		template<typename T>
		T get(const std::string& name)
		{
			st_mysql_column& column = get_column(name);
			if (column.is_null)
			{
				throw exception(__FILE__, __LINE__, "null value field");
			}

			T value = T();
			fetch_column(column, value);

			return value;
		}

		void fetch_column(const st_mysql_column& column, unsigned char& value);

		void fetch_column(const st_mysql_column& column, short int& value);
		void fetch_column(const st_mysql_column& column, unsigned short int& value);

		void fetch_column(const st_mysql_column& column, int& value);
		void fetch_column(const st_mysql_column& column, unsigned int& value);

		void fetch_column(const st_mysql_column& column, long long int& value);
		void fetch_column(const st_mysql_column& column, unsigned long long int& value);

		void fetch_column(const st_mysql_column& column, float& value);
		void fetch_column(const st_mysql_column& column, double& value);

		void fetch_column(const st_mysql_column& column, std::string& value);

		bool is_null(unsigned int index);
		bool is_null(const std::string& name);

	private:
		bool fetch_stmt_result();
		bool fetch_proc_result();

		st_mysql_column& get_column(unsigned int index);
		st_mysql_column& get_column(const std::string& name);

		st_mysql_stmt* stmt;
		st_mysql_res* metadata;
		st_mysql_field* fields;

		unsigned int field_count;

		std::vector<st_mysql_bind> binds;
		std::vector<st_mysql_column> columns;
	};

	class statement
	{
	public:
		statement(st_mysql* mysql, const std::string& query);
		~statement();

		void param(const unsigned char& value);

		void param(const short int& value);
		void param(const unsigned short int& value);

		void param(const int& value);
		void param(const unsigned int& value);

		void param(const long long int& value);
		void param(const unsigned long long int& value);

		void param(const float& value);
		void param(const double& value);

		void param(const std::string& value);
		void param_blob(const std::string& value);
		void param_null(char is_null = 1);

		unsigned long long int execute();
		result* query();

	private:
		st_mysql_bind& get_bind();

		st_mysql_stmt* stmt;

		int param_count;
		int bind_index;

		std::vector<st_mysql_bind> binds;
		std::vector<unsigned long> lengths;
	};

	class connection
	{
	public:
		connection(const std::string& host, const std::string& userid, const std::string& passwd, const std::string& database, unsigned int port = 3306, const std::string& charset = "utf8", bool pooled_ = false);
		~connection();

		bool ping();
		statement* prepare(const std::string& query);

		std::tm* last_released()
		{
			return &released;
		}

		void set_released(const std::tm& released_)
		{
			released = released_;
		}

		bool is_pooled()
		{
			return pooled;
		}

		void set_pooled(bool pooled_)
		{
			pooled = pooled_;
		}

	private:
		st_mysql* mysql;

		std::tm released;
		bool pooled;

	};

} // namespace mysqlpp

#endif // MYSQLPP_HPP
