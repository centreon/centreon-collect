/**
 * Copyright 2026 Centreon
 *
 * This file is part of Centreon Engine.
 *
 * Centreon Engine is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * Centreon Engine is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Centreon Engine. If not, see
 * <http://www.gnu.org/licenses/>.
 */

#ifndef CCE_TIMEPERIODS_TIMEPERIOD_TYPES_HH
#define CCE_TIMEPERIODS_TIMEPERIOD_TYPES_HH

/* Date range types. */
#define DATERANGE_CALENDAR_DATE 0  /* 2008-12-25 */
#define DATERANGE_MONTH_DATE 1     /* july 4 (specific month) */
#define DATERANGE_MONTH_DAY 2      /* day 21 (generic month) */
#define DATERANGE_MONTH_WEEK_DAY 3 /* 3rd thursday (specific month) */
#define DATERANGE_WEEK_DAY 4       /* 3rd thursday (generic month) */
#define DATERANGE_TYPES 5

#endif /* ! CCE_TIMEPERIODS_TIMEPERIOD_TYPES_HH */
