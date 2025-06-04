#ifndef CENTREON_POSIX_PROCESS_LAUNCHER_HH
#define CENTREON_POSIX_PROCESS_LAUNCHER_HH

#include <boost/process/v2/posix/default_launcher.hpp>
#include <boost/process/v2/stdio.hpp>

namespace boost::process::v2::posix {

struct centreon_posix_default_launcher;

struct centreon_process_stdio {
  boost::process::v2::detail::process_input_binding in;
  boost::process::v2::detail::process_output_binding out;
  boost::process::v2::detail::process_error_binding err;

  error_code on_exec_setup(centreon_posix_default_launcher& launcher
                           [[maybe_unused]],
                           const filesystem::path&,
                           const char* const*) {
    if (::dup2(in.fd, in.target) == -1)
      return error_code(errno, system_category());

    if (::dup2(out.fd, out.target) == -1)
      return error_code(errno, system_category());

    if (::dup2(err.fd, err.target) == -1)
      return error_code(errno, system_category());

    return error_code{};
  };
};

/**
 * This class is a copy of posix::default_launcher
 * as io_context::notify_fork can hang on child process and as we don't care
 * about child process in asio as we will do an exec, it's removed
 */
struct centreon_posix_default_launcher {
  /// The pointer to the environment forwarded to the subprocess.
  const char* const* env = ::environ;
  /// The pid of the subprocess - will be assigned after fork.
  int pid = -1;

  /// The whitelist for file descriptors.
  std::vector<int> fd_whitelist = {STDIN_FILENO, STDOUT_FILENO, STDERR_FILENO};

  centreon_posix_default_launcher() = default;

  template <typename ExecutionContext, typename Args, typename... Inits>
  auto operator()(
      ExecutionContext& context,
      const typename std::enable_if<
          std::is_convertible<ExecutionContext&,
                              boost::asio::execution_context&>::value,
          filesystem::path>::type& executable,
      Args&& args,
      Inits&&... inits)
      -> basic_process<typename ExecutionContext::executor_type> {
    error_code ec;
    auto proc = (*this)(context, ec, executable, std::forward<Args>(args),
                        std::forward<Inits>(inits)...);

    if (ec)
      v2::detail::throw_error(ec, "centreon_posix_default_launcher");

    return proc;
  }

  template <typename ExecutionContext, typename Args, typename... Inits>
  auto operator()(
      ExecutionContext& context,
      error_code& ec [[maybe_unused]],
      const typename std::enable_if<
          std::is_convertible<ExecutionContext&,
                              boost::asio::execution_context&>::value,
          filesystem::path>::type& executable,
      Args&& args,
      Inits&&... inits)
      -> basic_process<typename ExecutionContext::executor_type> {
    return (*this)(context.get_executor(), executable, std::forward<Args>(args),
                   std::forward<Inits>(inits)...);
  }

  template <typename Executor, typename Args, typename... Inits>
  auto operator()(Executor exec,
                  const typename std::enable_if<
                      boost::asio::execution::is_executor<Executor>::value ||
                          boost::asio::is_executor<Executor>::value,
                      filesystem::path>::type& executable,
                  Args&& args,
                  Inits&&... inits) -> basic_process<Executor> {
    error_code ec;
    auto proc =
        (*this)(std::move(exec), ec, executable, std::forward<Args>(args),
                std::forward<Inits>(inits)...);

    if (ec)
      v2::detail::throw_error(ec, "centreon_posix_default_launcher");

    return proc;
  }

  template <typename Executor, typename Args, typename... Inits>
  auto operator()(Executor exec,
                  error_code& ec,
                  const typename std::enable_if<
                      boost::asio::execution::is_executor<Executor>::value ||
                          boost::asio::is_executor<Executor>::value,
                      filesystem::path>::type& executable,
                  Args&& args,
                  Inits&&... inits) -> basic_process<Executor> {
    auto argv = this->build_argv_(executable, std::forward<Args>(args));
    {
      pipe_guard pg;
      if (::pipe(pg.p)) {
        BOOST_PROCESS_V2_ASSIGN_EC(ec, errno, system_category());
        return basic_process<Executor>{exec};
      }
      if (::fcntl(pg.p[1], F_SETFD, FD_CLOEXEC)) {
        BOOST_PROCESS_V2_ASSIGN_EC(ec, errno, system_category());
        return basic_process<Executor>{exec};
      }
      ec = detail::on_setup(*this, executable, argv, inits...);
      if (ec) {
        detail::on_error(*this, executable, argv, ec, inits...);
        return basic_process<Executor>(exec);
      }
      fd_whitelist.push_back(pg.p[1]);

      auto& ctx = boost::asio::query(exec, boost::asio::execution::context);
      ctx.notify_fork(boost::asio::execution_context::fork_prepare);
      pid = ::fork();
      if (pid == -1) {
        ctx.notify_fork(boost::asio::execution_context::fork_parent);
        detail::on_fork_error(*this, executable, argv, ec, inits...);
        detail::on_error(*this, executable, argv, ec, inits...);

        BOOST_PROCESS_V2_ASSIGN_EC(ec, errno, system_category());
        return basic_process<Executor>{exec};
      } else if (pid == 0) {
        ::close(pg.p[0]);
        /**
         * ctx.notify_fork calls epoll_reactor::notify_fork which locks
         * registered_descriptors_mutex_ An issue occurs when
         * registered_descriptors_mutex_ is locked by another thread at fork
         * timepoint. In such a case, child process starts with
         * registered_descriptors_mutex_ already locked and both child and
         * parent process will hang.
         */
        // ctx.notify_fork(boost::asio::execution_context::fork_child);
        ec = detail::on_exec_setup(*this, executable, argv, inits...);
        if (!ec) {
          close_all_fds(ec);
        }
        if (!ec)
          ::execve(executable.c_str(), const_cast<char* const*>(argv),
                   const_cast<char* const*>(env));

        ignore_unused(::write(pg.p[1], &errno, sizeof(int)));
        BOOST_PROCESS_V2_ASSIGN_EC(ec, errno, system_category());
        detail::on_exec_error(*this, executable, argv, ec, inits...);
        ::exit(EXIT_FAILURE);
        return basic_process<Executor>{exec};
      }

      ctx.notify_fork(boost::asio::execution_context::fork_parent);
      ::close(pg.p[1]);
      pg.p[1] = -1;
      int child_error{0};
      int count = -1;
      while ((count = ::read(pg.p[0], &child_error, sizeof(child_error))) ==
             -1) {
        int err = errno;
        if ((err != EAGAIN) && (err != EINTR)) {
          BOOST_PROCESS_V2_ASSIGN_EC(ec, err, system_category());
          break;
        }
      }
      if (count != 0)
        BOOST_PROCESS_V2_ASSIGN_EC(ec, child_error, system_category());

      if (ec) {
        if (pid > 0) {
          ::kill(pid, SIGKILL);
          ::waitpid(pid, nullptr, 0);
        }
        detail::on_error(*this, executable, argv, ec, inits...);
        return basic_process<Executor>{exec};
      }
    }
    basic_process<Executor> proc(exec, pid);
    detail::on_success(*this, executable, argv, ec, inits...);
    return proc;
  }

 protected:
  void ignore_unused(std::size_t) {}
  void close_all_fds(error_code& ec) {
    std::sort(fd_whitelist.begin(), fd_whitelist.end());
    detail::close_all(fd_whitelist, ec);
    fd_whitelist = {STDIN_FILENO, STDOUT_FILENO, STDERR_FILENO};
  }

  struct pipe_guard {
    int p[2];
    pipe_guard() : p{-1, -1} {}

    ~pipe_guard() {
      if (p[0] != -1)
        ::close(p[0]);
      if (p[1] != -1)
        ::close(p[1]);
    }
  };

  // if we need to allocate something
  std::vector<std::string> argv_buffer_;
  std::vector<const char*> argv_;

  template <typename Args>
  const char* const* build_argv_(
      const filesystem::path& pt,
      const Args& args,
      typename std::enable_if<
          std::is_convertible<decltype(*std::begin(std::declval<Args>())),
                              cstring_ref>::value>::type* = nullptr) {
    const auto arg_cnt = std::distance(std::begin(args), std::end(args));
    argv_.reserve(arg_cnt + 2);
    argv_.push_back(pt.native().data());
    for (auto&& arg : args)
      argv_.push_back(arg.c_str());

    argv_.push_back(nullptr);
    return argv_.data();
  }

  const char* const* build_argv_(const filesystem::path&, const char** argv) {
    return argv;
  }

  template <typename Args>
  const char* const* build_argv_(
      const filesystem::path& pt,
      const Args& args,
      typename std::enable_if<
          !std::is_convertible<decltype(*std::begin(std::declval<Args>())),
                               cstring_ref>::value>::type* = nullptr) {
    const auto arg_cnt = std::distance(std::begin(args), std::end(args));
    argv_.reserve(arg_cnt + 2);
    argv_buffer_.reserve(arg_cnt);
    argv_.push_back(pt.native().data());

    using char_type =
        typename decay<decltype((*std::begin(std::declval<Args>()))[0])>::type;

    for (basic_string_view<char_type> arg : args)
      argv_buffer_.push_back(
          v2::detail::conv_string<char>(arg.data(), arg.size()));

    for (auto&& arg : argv_buffer_)
      argv_.push_back(arg.c_str());

    argv_.push_back(nullptr);
    return argv_.data();
  }
};

}  // namespace boost::process::v2::posix

#endif
