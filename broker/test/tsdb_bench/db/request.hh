#ifndef __TSDB__BENCH__REQUEST_HH
#define __TSDB__BENCH__REQUEST_HH

class connection;

class expandable_buffer {
 protected:
  uint8_t* _buffer;
  size_t _size;
  size_t _allocated;

  void reserve(size_t byte_size);

 public:
  expandable_buffer();
  ~expandable_buffer();

  size_t size() const { return _size; }

  void append(const void* data, size_t byte_size);
  void append8(uint8_t data);
  void hton_append16(uint16_t data);
  void hton_append32(uint32_t data);
  void hton_append64(uint64_t data);
  constexpr const unsigned char* get_buffer() const { return _buffer; }
};

class request_base : public std::enable_shared_from_this<request_base> {
 public:
  using pointer = std::shared_ptr<request_base>;
  using callback = std::function<
      void(const std::error_code&, const std::string&, const pointer&)>;

  enum class e_request_type {
    simple_no_result_request,
    load_request,
    statement_request
  };

  enum class e_column_type { int64_c, timestamp_c, double_c, float_c };

 protected:
  e_request_type _type;
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

  virtual int send_query(connection&) = 0;

  void dump(std::ostream& s) const override;

  constexpr const std::string& get_request() const { return _request; }
};

template <class callback_type>
no_result_request::no_result_request(const std::string& req,
                                     callback_type&& callback)
    : request_base(e_request_type::simple_no_result_request, callback),
      _request(req) {}

class load_request : public no_result_request {
 protected:
  std::vector<e_column_type> _columns;
  size_t _nb_row;

 public:
  struct null_value {};

  template <typename column_iterator, class callback_type>
  load_request(const std::string& req,
               size_t nb_row,
               column_iterator begin,
               column_iterator end,
               callback_type&& callback);

  constexpr size_t get_nb_row() const { return _nb_row; }
  size_t get_nb_columns() const { return _columns.size(); }

  virtual void in(const time_point&) = 0;
  virtual void in(double) = 0;
  virtual void in(float) = 0;
  virtual void in(int64_t) = 0;
  virtual void in(uint64_t) = 0;
  virtual void in(const null_value&) = 0;
};

template <typename column_iterator, class callback_type>
load_request::load_request(const std::string& req,
                           size_t nb_row,
                           column_iterator begin,
                           column_iterator end,
                           callback_type&& callback)
    : no_result_request(req, callback), _columns(begin, end), _nb_row(nb_row) {
  _type = e_request_type::load_request;
}

template <typename data_type>
inline load_request& operator<<(load_request& req, const data_type& data) {
  req.in(data);
  return req;
}

class no_result_statement_request : public no_result_request {
 protected:
  std::string _name;
  using in_type_cont = std::vector<e_column_type>;
  in_type_cont _in_types;  // this vect define all in data type

 public:
  struct null_value {};

  template <typename column_iterator, class callback_type>
  no_result_statement_request(const std::string& name,
                              const std::string& req,
                              column_iterator begin,
                              column_iterator end,
                              callback_type&& callback);

  virtual void in(const time_point&) = 0;
  virtual void in(double) = 0;
  virtual void in(float) = 0;
  virtual void in(int64_t) = 0;
  virtual void in(uint64_t) = 0;
  virtual void in(const null_value&) = 0;
};

/**
 * @brief Construct a new no result statement request::no result statement
 * request object if name & req aren't empty, it saves the statement in server
 * if only name isn't empty, it executes a statement saved in the server
 * if only req is not null, it executes the statement
 * @tparam value_type_iterator
 * @tparam callback_type
 * @param name name of the statement
 * @param req request
 * @param begin iterator on e_column_type values
 * @param end  end iterator on e_column_type values
 * @param callback callback called on completion
 */
template <typename value_type_iterator, class callback_type>
no_result_statement_request::no_result_statement_request(
    const std::string& name,
    const std::string& req,
    value_type_iterator begin,
    value_type_iterator end,
    callback_type&& callback)
    : no_result_request(req, callback), _name(name), _in_types(begin, end) {
  _type = e_request_type::statement_request;
}

template <typename data_type>
inline no_result_statement_request& operator<<(no_result_statement_request& req,
                                               const data_type& data) {
  req.in(data);
  return req;
}

#endif
