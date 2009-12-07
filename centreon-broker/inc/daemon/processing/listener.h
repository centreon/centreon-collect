/*
**  Copyright 2009 MERETHIS
**  This file is part of CentreonBroker.
**
**  CentreonBroker is free software: you can redistribute it and/or modify it
**  under the terms of the GNU General Public License as published by the Free
**  Software Foundation, either version 2 of the License, or (at your option)
**  any later version.
**
**  CentreonBroker is distributed in the hope that it will be useful, but
**  WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
**  or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
**  for more details.
**
**  You should have received a copy of the GNU General Public License along
**  with CentreonBroker.  If not, see <http://www.gnu.org/licenses/>.
**
**  For more information : contact@centreon.com
*/

#ifndef PROCESSING_LISTENER_H_
# define PROCESSING_LISTENER_H_

# include <memory>               // for auto_ptr
# include "concurrency/mutex.h"
# include "concurrency/thread.h"
# include "io/acceptor.h"

namespace                       Processing
{
  /**
   *  \class Listener listener.h "processing/listener.h"
   *  \brief Waits for incoming connections on an acceptor.
   *
   *  The Listener class implements a mediator pattern and avoid acceptors from
   *  being tightly coupled to the Manager and Feeder.
   */
  class                         Listener : public Concurrency::Thread
  {
   public:
    enum                        Protocol
    {
      UNKNOWN = 0,
      NDO,
      XML
    };

   private:
    std::auto_ptr<IO::Acceptor> acceptor_;
    bool                        init_;
    Concurrency::Mutex          initm_;
    Protocol                    protocol_;
                                Listener(const Listener& listener);
    Listener&                   operator=(const Listener& listener);

   public:
                                Listener();
                                ~Listener();
    void                        operator()();
    void                        Init(IO::Acceptor* acceptor,
                                     Protocol proto,
                                     Concurrency::ThreadListener* tl = NULL);
  };
}

#endif /* !PROCESSING_LISTENER_H_ */
