#ifndef __TSDB__BENCH__REQUEST_HH
#define __TSDB__BENCH__REQUEST_HH

class request_base : public std::enable_shared_from_this<request_base> {
 public:
  using pointer = std::shared_ptr<request_base>;
  using callback = std::function<
      void(const std::error_code&, const std::string&, const pointer&)>;

  enum class e_request_type { simple_no_result_request };

 protected:
  const e_request_type _type;
  std::atomic_bool _callback_called;
  callback _callback;

 public:
  template <class callback_type>
  request_base(e_request_type request_type, callback_type&& callback);

  virtual ~request_base() = default;

  constexpr e_request_type get_type() const { return _type; }

  virtual void dump(std::ostream& s) const;
  void call_callback(const std::error_code& err, const std::string& err_detail);
};

template <class callback_type>
request_base::request_base(e_request_type request_type,
                           callback_type&& callback)
    : _type(request_type), _callback_called(false), _callback(callback) {}

std::ostream& operator<<(std::ostream& s, const request_base& req);
std::ostream& operator<<(std::ostream& s,
                         const request_base::e_request_type& req_type);

class no_result_request : public request_base {
 protected:
  std::string _request;

 public:
  template <class callback_type>
  no_result_request(const std::string& req, callback_type&& callback);

  void dump(std::ostream& s) const override;

  const std::string& get_request() { return _request; }
};

template <class callback_type>
no_result_request::no_result_request(const std::string& req,
                                     callback_type&& callback)
    : request_base(e_request_type::simple_no_result_request, callback),
      _request(req) {}

#endif
