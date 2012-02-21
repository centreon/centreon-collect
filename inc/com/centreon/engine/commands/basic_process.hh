/*
** Copyright 2011 Merethis
**
** This file is part of Centreon Engine.
**
** Centreon Engine is free software: you can redistribute it and/or
** modify it under the terms of the GNU General Public License version 2
** as published by the Free Software Foundation.
**
** Centreon Engine is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with Centreon Engine. If not, see
** <http://www.gnu.org/licenses/>.
*/

#ifndef CCE_COMMANDS_BASIC_PROCESS_HH
# define CCE_COMMANDS_BASIC_PROCESS_HH

# include <QIODevice>
# include <list>
# include <string>
# include <string>
# include <QProcess>
# include <QSocketNotifier>
# include <QSharedPointer>

namespace                          com {
  namespace                        centreon {
    namespace                      engine {
      namespace                    commands {
        /**
         *  @class basic_process commands/basic_proces.hh
         *  @brief Basic process is a reimplementation of QProcess
         *  without use select function.
         *
         *  Basic process provide a simple interface to execute
         *  some process.
         */
        class                      basic_process : public QIODevice {
          Q_OBJECT
        public:
          basic_process(QObject* parent = 0);
          virtual ~basic_process() throw();

          void                     closeReadChannel(QProcess::ProcessChannel channel);
          void                     closeWriteChannel();
          // std::list<std::string>              environment() const; // (deprecated)
          QProcess::ProcessError   error() const;
          int                      exitCode() const;
          QProcess::ExitStatus     exitStatus() const;
          // std::string                  nativeArguments() const;
          Q_PID                    pid() const;
          // ProcessChannelMode       processChannelMode() const;
          // QProcessEnvironment      processEnvironment() const;
          std::string               readAllStandardError();
          std::string               readAllStandardOutput();
          QProcess::ProcessChannel readChannel() const;
          // void                     setEnvironment(std::list<std::string> const& environment); // (deprecated)
          // void                     setNativeArguments(std::string const& arguments);
          // void                     setProcessChannelMode(ProcessChannelMode mode);
          // void                     setProcessEnvironment(QProcessEnvironment const& environment);
          void                     setReadChannel(QProcess::ProcessChannel channel);
          // void                     setStandardErrorFile(std::string const& fileName, OpenMode mode = Truncate);
          // void                     setStandardInputFile(std::string const& fileName);
          // void                     setStandardOutputFile(std::string const& fileName, OpenMode mode = Truncate);
          // void                     setStandardOutputProcess(QProcess* destination);
          void                     setWorkingDirectory(std::string const& dir);
          void                     start(std::string const& program, std::list<std::string> const& arguments, OpenMode mode = ReadWrite);
          void                     start(std::string const& program, OpenMode mode = ReadWrite);
          QProcess::ProcessState   state() const;
          bool                     waitForFinished(int msecs = 30000);
          bool                     waitForReadyRead(int msecs = 30000);
          bool                     waitForBytesWritten(int msecs = 30000);
          bool                     waitForStarted(int msecs = 30000);
          std::string                  workingDirectory() const;
          qint64                   writeData(std::string const& data);

          // QIODevice
          qint64                   bytesAvailable() const;
          qint64                   bytesToWrite() const;
          bool                     isSequential() const;
          bool                     canReadLine() const;
          void                     close();
          bool                     atEnd() const;

        signals:
          void                     error(QProcess::ProcessError error);
          void                     finished(int exitCode, QProcess::ExitStatus exitStatus);
          void                     readyReadStandardError();
          void                     readyReadStandardOutput();
          void                     started();
          void                     stateChanged(QProcess::ProcessState newState);

        public slots:
          void                     kill();
          void                     terminate();

        protected:
          void                     setProcessState(QProcess::ProcessState state);
          virtual void             setupChildProcess();

          // QIODevice
          virtual qint64           readData(char* data, qint64 maxlen);
          virtual qint64           writeData(char const* data, qint64 len);

        private slots:
          void                     _notification_standard_output();
          void                     _notification_standard_error();
          void                     _notification_dead();

        private:
          basic_process(basic_process const&);
          basic_process& operator=(basic_process const&);

          void                     _start_process(OpenMode mode);
          void                     _exec_child();
          void                     _close_pipe() throw();
          void                     _emit_finished();
          static bool              _read(int fd, std::string* str);
          static qint64            _available_bytes(int fd) throw();
          static pid_t             _waitpid(pid_t pid, int* status, int options) throw();
          static qint64            _read(int fd, void* buf, qint64 nbyte) throw();
          static void              _close(int& fd) throw();
          static int               _chdir(char const* working_directory) throw();
          static int               _dup2(int fildes, int fildes2) throw();
          static char**            _build_args(std::string const& program, std::list<std::string> const& arguments);
          static std::list<std::string>       _split_command_line(std::string const& command_line);

          // QProcessEnvironment      _environment;
          std::list<std::string>              _arguments;
          std::string                  _program;
          std::string                  _working_directory;
          std::string               _standard_output;
          std::string               _standard_error;
          QSocketNotifier*         _notifier_output;
          QSocketNotifier*         _notifier_error;
          QSocketNotifier*         _notifier_dead;
          QProcess::ProcessChannel _channel;
          QProcess::ProcessError   _perror;
          QProcess::ProcessState   _pstate;
          pid_t                    _pid;
          int                      _pipe_out[2];
          int                      _pipe_err[2];
          int                      _pipe_in[2];
          int                      _pipe_dead[2];
          int                      _status;
        };
      }
    }
  }
}

#endif // !CCE_COMMANDS_BASIC_PROCESS_HH
