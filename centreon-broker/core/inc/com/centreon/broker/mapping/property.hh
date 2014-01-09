/*
** Copyright 2011 Merethis
** This file is part of Centreon Broker.
**
** Centreon Broker is free software: you can redistribute it and/or
** modify it under the terms of the GNU General Public License version 2
** as published by the Free Software Foundation.
**
** Centreon Broker is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with Centreon Broker. If not, see
** <http://www.gnu.org/licenses/>.
*/

#ifndef CCB_MAPPING_PROPERTY_HH_
# define CCB_MAPPING_PROPERTY_HH_

# include "com/centreon/broker/mapping/source.hh"

namespace                  com {
  namespace                centreon {
    namespace              broker {
      namespace            mapping {
        /**
         *  @class property property.hh "com/centreon/broker/mapping/property.hh"
         *  @brief Internal property-mapping class.
         *
         *  This class is used internally by the mapping engine and
         *  should not be used otherwise.
         */
        template           <typename T>
        class              property : public source {
         private:
          union {
            bool           (T::* b);
            double         (T::* d);
            int            (T::* i);
            short          (T::* s);
            QString        (T::* q);
            time_t         (T::* t);
            unsigned int   (T::* I);
            unsigned short (T::* S);
          }                _prop;

         public:
          /**
           *  Boolean constructor.
           *
           *  @param[in]  b Boolean property.
           *  @param[out] t If not NULL, set to BOOL.
           */
                           property(bool (T::* b), source_type* t) {
            _prop.b = b;
            if (t)
              *t = BOOL;
          }

          /**
           *  Double constructor.
           *
           *  @param[in]  d Double property.
           *  @param[out] t If not NULL, set to DOUBLE.
           */
                           property(double (T::* d), source_type* t) {
            _prop.d = d;
            if (t)
              *t = DOUBLE;
          }

          /**
           *  Integer constructor.
           *
           *  @param[in]  i Integer property.
           *  @param[out] t If not NULL, set to INT.
           */
                           property(int (T::* i), source_type* t) {
            _prop.i = i;
            if (t)
              *t = INT;
          }

          /**
           *  Short constructor.
           *
           *  @param[in]  s Short property.
           *  @param[out] t If not NULL, set to SHORT.
           */
                           property(short (T::* s), source_type* t) {
            _prop.s = s;
            if (t)
              *t = SHORT;
          }

          /**
           *  String constructor.
           *
           *  @param[in]  q String property.
           *  @param[out] t If not NULL, set to STRING.
           */
                           property(QString (T::* q), source_type* t) {
            _prop.q = q;
            if (t)
              *t = STRING;
          }

          /**
           *  Time constructor.
           *
           *  @param[in]  tt Time property.
           *  @param[out] t  If not NULL, set to TIME.
           */
                           property(time_t (T::* tt), source_type* t) {
            _prop.t = tt;
            if (t)
              *t = TIME;
          }

          /**
           *  Unsigned integer constructor.
           *
           *  @param[in]  I Unsigned integer property.
           *  @param[out] t If not NULL, set to UINT.
           */
                           property(unsigned int (T::* I), source_type* t) {
            _prop.I = I;
            if (t)
              *t = UINT;
          }

          /**
           *  Unsigned short constructor.
           *
           *  @param[in]  S Unsigned short property.
           *  @param[out] t If not NULL, set to USHORT.
           */
                           property(unsigned short (T::* S), source_type* t) {
            _prop.S = S;
            if (t)
              *t = USHORT;
          }

          /**
           *  Copy constructor.
           *
           *  @param[in] p Object to copy.
           */
                           property(property const& p)
            : source(p), _prop(p._prop) {
          }

          /**
           *  Destructor.
           */
                           ~property() {}

          /**
           *  Assignment operator.
           *
           *  @param[in] p Object to copy.
           *
           *  @return This object.
           */
          property&        operator=(property const& p) {
            _prop = p._prop;
            return (*this);
          }

          /**
           *  Get a boolean property.
           *
           *  @param[in] d Object to get from.
           *
           *  @return Boolean property.
           */
          bool             get_bool(io::data const& d) {
            return (static_cast<T const*>(&d)->*(_prop.b));
          }

          /**
           *  Get a double property.
           *
           *  @param[in] d Object to get from.
           *
           *  @return Double property.
           */
          double           get_double(io::data const& d) {
            return (static_cast<T const*>(&d)->*(_prop.d));
          }

          /**
           *  Get an integer property.
           *
           *  @param[in] d Object to get from.
           *
           *  @return Integer property.
           */
          int              get_int(io::data const& d) {
            return (static_cast<T const*>(&d)->*(_prop.i));
          }

          /**
           *  Get a short property.
           *
           *  @param[in] d Object to get from.
           *
           *  @return Short property.
           */
          short            get_short(io::data const& d) {
            return (static_cast<T const*>(&d)->*(_prop.s));
          }

          /**
           *  Get a string property.
           *
           *  @param[in] d Object to get from.
           *
           *  @return String property.
           */
          QString const&   get_string(io::data const& d) {
            return (static_cast<T const*>(&d)->*(_prop.q));
          }

          /**
           *  Get a time property.
           *
           *  @param[in] d Object to get from.
           *
           *  @return Time property.
           */
          time_t           get_time(io::data const& d) {
            return (static_cast<T const*>(&d)->*(_prop.t));
          }

          /**
           *  Get an unsigned integer property.
           *
           *  @param[in] d Object to get from.
           *
           *  @return Unsigned integer property.
           */
          unsigned int     get_uint(io::data const& d) {
            return (static_cast<T const*>(&d)->*(_prop.I));
          }

          /**
           *  Get an unsigned short property.
           *
           *  @param[in] d Object to get from.
           *
           *  @return Unsigned short property.
           */
          unsigned short   get_ushort(io::data const& d) {
            return (static_cast<T const*>(&d)->*(_prop.S));
          }

          /**
           *  Set a boolean property.
           *
           *  @param[out] d     Object to set.
           *  @param[in]  value New value.
           */
          void             set_bool(io::data& d, bool value) {
            static_cast<T*>(&d)->*(_prop.b) = value;
            return ;
          }

          /**
           *  Set a double property.
           *
           *  @param[out] d     Object to set.
           *  @param[in]  value New value.
           */
          void             set_double(io::data& d, double value) {
            static_cast<T*>(&d)->*(_prop.d) = value;
            return ;
          }

          /**
           *  Set an integer property.
           *
           *  @param[out] d     Object to set.
           *  @param[in]  value New value.
           */
          void             set_int(io::data& d, int value) {
            static_cast<T*>(&d)->*(_prop.i) = value;
            return ;
          }

          /**
           *  Set a short property.
           *
           *  @param[out] d     Object to set.
           *  @param[in]  value New value.
           */
          void             set_short(io::data& d, short value) {
            static_cast<T*>(&d)->*(_prop.s) = value;
            return ;
          }

          /**
           *  Set a string property.
           *
           *  @param[out] d     Object to set.
           *  @param[in]  value New value.
           */
          void             set_string(io::data& d,
                                      QString const& value) {
            static_cast<T*>(&d)->*(_prop.q) = value;
            return ;
          }

          /**
           *  Set a time property.
           *
           *  @param[out] d     Object to set.
           *  @param[in]  value New value.
           */
          void             set_time(io::data& d, time_t value) {
            static_cast<T*>(&d)->*(_prop.t) = value;
            return ;
          }

          /**
           *  Set an unsigned integer property.
           *
           *  @param[out] d     Object to set.
           *  @param[in]  value New value.
           */
          void             set_uint(io::data& d, unsigned int value) {
            static_cast<T*>(&d)->*(_prop.I) = value;
            return ;
          }

          /**
           *  Set an unsigned short property.
           *
           *  @param[out] d     Object to set.
           *  @param[in]  value New value.
           */
          void             set_ushort(io::data& d,
                                      unsigned short value) {
            static_cast<T*>(&d)->*(_prop.S) = value;
            return ;
          }
        };
      }
    }
  }
}

#endif /* !CCB_MAPPING_PROPERTY_HH_ */
