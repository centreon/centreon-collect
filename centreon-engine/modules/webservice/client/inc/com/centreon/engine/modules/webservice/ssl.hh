/*
** Copyright (C) 2000-2007, Robert van Engelen, Genivia Inc. All Rights Reserved.
** Copyright 2011-2013 Merethis
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

#ifndef CCE_MOD_WS_SSL_HH
# define CCE_MOD_WS_SSL_HH

#  ifdef WITH_OPENSSL
int CRYPTO_thread_setup();
void CRYPTO_thread_cleanup();
# endif // !WITH_OPENSSL

#endif // !CCE_MOD_WS_SSL_HH
