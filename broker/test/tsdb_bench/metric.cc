#include "precomp_inc.hh"

#include "metric.hh"

constexpr float _randmax = RAND_MAX;

static std::random_device _shuffle_rd;
static std::mt19937 _shuffle_gg(_shuffle_rd());

metric::metric_cont_ptr metric::create_metrics(const metric_conf& conf) {
  unsigned metric_ind = 0, host_index = 1, service_index = 1, metric_id_ind = 1;
  // from 0 to float_end, create float value
  unsigned float_end = conf.float_percent;
  // from float_end to double_end, create double value
  unsigned double_end = float_end + conf.double_percent;
  // from double_end to int64_end, create int64_t value
  unsigned int64_end = double_end + conf.int64_percent;
  // from int64_end to 100 create uint64_t value
  unsigned metric_by_host = conf.metric_nb / conf.nb_host;
  if (!metric_by_host) {
    metric_by_host = 1;
  }
  unsigned metric_by_service =
      conf.metric_nb / (conf.nb_host * conf.nb_service_by_host);
  if (!metric_by_service) {
    metric_by_service = 1;
  }
  unsigned metric_by_id =
      conf.metric_nb /
      (conf.nb_host * conf.nb_service_by_host * conf.metric_id_nb);
  if (!metric_by_id) {
    metric_by_id = 1;
  }
  metric_cont_ptr ret(std::make_shared<metric_cont>());
  metric_cont& to_fill(*ret);
  to_fill.reserve(conf.metric_nb);
  for (; metric_ind < conf.metric_nb; ++metric_ind) {
    if (!metric_ind % metric_by_host) {
      ++host_index;
    }
    if (!metric_ind % metric_by_service) {
      ++service_index;
    }
    if (!metric_ind % metric_by_id) {
      ++metric_id_ind;
    }

    unsigned metric_type = metric_ind % 100;
    if (metric_type < float_end) {
      to_fill.emplace_back(service_index, host_index, metric_id_ind,
                           system_clock::now(), (float)rand() / RAND_MAX);
    } else if (metric_type < double_end) {
      to_fill.emplace_back(service_index, host_index, metric_id_ind,
                           system_clock::now(), (double)rand() / RAND_MAX);
    } else if (metric_type < int64_end) {
      to_fill.emplace_back(service_index, host_index, metric_id_ind,
                           system_clock::now(), (int64_t)rand());
    } else {
      to_fill.emplace_back(service_index, host_index, metric_id_ind,
                           system_clock::now(), (uint64_t)rand());
    }
  }

  std::shuffle(to_fill.begin(), to_fill.end(), _shuffle_gg);

  return ret;
}
