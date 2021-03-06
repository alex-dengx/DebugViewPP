// (C) Copyright Gert-Jan de Vos and Jan Wilmans 2013.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at 
// http://www.boost.org/LICENSE_1_0.txt)

// Repository at: https://github.com/djeedjay/DebugViewPP/

#pragma once

#include <string>
#include <fstream>
#include <boost/thread.hpp>

namespace fusion {
namespace debugviewpp {

std::ostream& operator<<(std::ostream& os, const FILETIME& ft);

class LogFile;

class FileWriter
{
public:
	FileWriter(const std::wstring& filename, LogFile& logfile);

private:
	void Run();
	
	std::ofstream m_ofstream;
	LogFile& m_logfile;
	boost::thread m_thread;
};

} // namespace debugviewpp 
} // namespace fusion
