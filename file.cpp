#include "StdAfx.h"
#include "file.h"
#include <codecvt>

std::string File::exec(std::wstring commandLine, const nonstd::optional<std::string> &str)
{
  std::string result;

  if (!m_file.empty())
  {
    commandLine += ' ';
    commandLine += '"';
    commandLine += m_file;
    commandLine += '"';
  }

  PROCESS_INFORMATION procInfo = {0};
  STARTUPINFO startInfo = {0};
  BOOL isSuccess = FALSE;

  HANDLE IN_Rd(0), IN_Wr(0), OUT_Rd(0), OUT_Wr(0), ERR_Rd(0), ERR_Wr(0);

  SECURITY_ATTRIBUTES security;

  security.nLength = sizeof(SECURITY_ATTRIBUTES);
  security.bInheritHandle = TRUE;
  security.lpSecurityDescriptor = NULL;

  CreatePipe(&OUT_Rd, &OUT_Wr, &security, 0);
  CreatePipe(&ERR_Rd, &ERR_Wr, &security, 0);
  CreatePipe(&IN_Rd, &IN_Wr, &security, 0);

  SetHandleInformation(OUT_Rd, HANDLE_FLAG_INHERIT, 0);
  SetHandleInformation(ERR_Rd, HANDLE_FLAG_INHERIT, 0);
  SetHandleInformation(IN_Wr, HANDLE_FLAG_INHERIT, 0);

  startInfo.cb = sizeof(STARTUPINFO);
  startInfo.hStdError = ERR_Wr;
  startInfo.hStdOutput = OUT_Wr;
  startInfo.hStdInput = IN_Rd;
  startInfo.dwFlags |= STARTF_USESTDHANDLES;

  isSuccess = CreateProcess(NULL,
    const_cast<wchar_t *>(commandLine.c_str()),  // command line
    NULL,                                        // process security attributes
    NULL,                                        // primary thread security attributes
    TRUE,                                        // handles are inherited
    CREATE_NO_WINDOW,                            // creation flags
    NULL,                                        // use parent's environment
    m_directory.c_str(),                         // use parent's current directory
    &startInfo,                                  // STARTUPINFO pointer
    &procInfo);                                  // receives PROCESS_INFORMATION

  if (!isSuccess)
  {
    //C++11 format converter
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> convert;
    std::string command = convert.to_bytes(commandLine);

    throw Linter::Exception("Linter: Can't execute command: " + command);
  }

  if (str.has_value())
  {
    const std::string &value = str.value();
    DWORD dwRead(static_cast<DWORD>(value.size())), dwWritten(0);
    WriteFile(IN_Wr, value.c_str(), dwRead, &dwWritten, nullptr);
  }

  CloseHandle(procInfo.hProcess);
  CloseHandle(procInfo.hThread);

  CloseHandle(ERR_Wr);
  CloseHandle(OUT_Wr);
  CloseHandle(IN_Wr);

  DWORD readBytes;
  std::string buffer;
  buffer.resize(4096);

  for (;;)
  {
    isSuccess = ReadFile(OUT_Rd, &buffer[0], static_cast<DWORD>(buffer.size()), &readBytes, NULL);
    if (!isSuccess || readBytes == 0)
    {
      break;
    }

    result += std::string(&buffer[0], readBytes);
  }

  return result;
}

File::File(const std::wstring &fileName, const std::wstring &directory): m_fileName(fileName), m_directory(directory)
{
};


File::~File()
{
  if (!m_file.empty())
  {
    _wunlink(m_file.c_str());
  }
}

bool File::write(const std::string &data)
{
  if (data.empty())
  {
    return false;
  }

  std::wstring tempFileName = m_directory + L"/" + m_fileName + L".temp.linter.file.tmp";

  HANDLE fileHandle = CreateFile(tempFileName.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_TEMPORARY, NULL);
  if (fileHandle == INVALID_HANDLE_VALUE)
  {
    return false;
  }

  DWORD bytes(0);
  WriteFile(fileHandle, &data[0], static_cast<DWORD>(data.size() * sizeof(data[0])), &bytes, 0);
  CloseHandle(fileHandle);

  m_file = tempFileName;

  return true;
}
