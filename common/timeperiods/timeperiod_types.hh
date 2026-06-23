/**
 * Copyright 2026 Centreon
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 *
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
