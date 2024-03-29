--package.path = package.path .. ";/home/admin/?.lua"
local mysql = require "luasql.mysql"

local blue = string.char(27) .. "[34m"
local red = string.char(27) .. "[31m"
local green = string.char(27) .. "[32m"
local yellow = string.char(27) .. "[33m"
local purple = string.char(27) .. "[35m"
local reset = string.char(27) .. "[0m"

local simu = {
  log_file = "/tmp/simu.log",
  host_count = 1,
  poller_count = 1,
  conn = nil,
  stack = {},
  step_build = 1,
  step_check = 1,
}

local step = {
  require('neb.instances'),                         --  1
  require('neb.hosts'),                             --  2
  require('neb.hostgroups'),                        --  3
  require('neb.hostgroup_members'),                 --  4
  require('neb.custom_variables'),                  --  5
  require('neb.custom_variable_status'),            --  6
  require('neb.services'),                          --  7
  require('neb.servicegroups'),                     --  8
  require('neb.servicegroup_members'),              --  9
  require('neb.service_checks'),                    -- 10
  require('neb.service_status'),                    -- 11
  require('neb.comments'),                          -- 12
  require('neb.downtimes'),                         -- 13
  require('neb.host_checks'),                       -- 14
  require('neb.host_status'),                       -- 15
  require('neb.acknowledgements'),                  -- 16
  require('neb.event_handler'),                     -- 17
  require('neb.flapping_status'),                   -- 18
  require('neb.host_dependency'),                   -- 19
  require('neb.host_parent'),                       -- 20
  require('neb.service_dependency'),                -- 21
  require('neb.instance_status'),                   -- 22
  require('neb.log'),                               -- 23
  require('bam.ba_status'),                         -- 24
  require('bam.kpi_status'),                        -- 25
  require('bam.ba_events'),                         -- 26
  require('bam.kpi_events'),                        -- 27
  require('bam.dimension_truncate_table_signal'),   -- 28
}

-- Instances                  => 18
step[1].count = {
  instance = 10,
  continue = true,
}

-- Hosts per instance         => 12
step[2].count = {
  host = 10,
  instance = step[1].count.instance,
  continue = true,
}

-- Hostgroups                 => 10
step[3].count = {
  group = 10,
  continue = true,
}

-- Hostgroups members         => 11
step[4].count = {
  host = step[2].count.host,
  instance = step[2].count.instance,
  hostgroup = 10,
  continue = true,
}

-- Custom variables per host  => 3
step[5].count = {
  cv = 30,
  host = step[2].count.host,
  instance = step[1].count.instance,
  continue = true,
}

-- Custom variables status per host  => 4
step[6].count = {
  cv = 30,
  host = step[2].count.host,
  instance = step[1].count.instance,
  continue = true,
}

-- Services per host          => 23
step[7].count = {
  service = 50,
  host = step[2].count.host,
  instance = step[2].count.instance,
  continue = false,
}

-- Servicegroups              => 21
step[8].count = {
  servicegroup = 20,
  continue = true,
}

-- Servicegroups members      => 22
step[9].count = {
  instance = step[2].count.instance,
  host = step[2].count.host,
  service = step[7].count.service,
  servicegroup = 20,
  continue = true,
}

-- Service checks             => 19
step[10].count = {
  service = step[7].count.service,
  host = step[2].count.host,
  instance = step[2].count.instance,
  continue = true,
}

-- Services status per host          => 24
step[11].count = {
  service = step[7].count.service,
  host = step[2].count.host,
  instance = step[2].count.instance,
  metric = 2,
  continue = true,
}

-- Comments per host
step[12].count = {
  comment = 50,
  host = step[2].count.host,
  instance = step[2].count.instance,
  continue = true,
  disabled = false,
}

-- Downtimes per host
step[13].count = {
  host = 5,
  disabled = false,
  continue = true,
}

-- Host checks and logs per instance
step[14].count = {
  host = step[2].count.host,
  instance = step[1].count.instance,
  continue = true,
}

-- Host status
step[15].count = {
  host = step[2].count.host,
  instance = step[1].count.instance,
  continue = true,
}

-- Acknowledgements
step[16].count = {
  service = step[7].count.service,
  host = step[2].count.host,
  instance = step[2].count.instance,
  continue = true,
}

-- Event handler
step[17].count = {
  service = step[7].count.service,
  host = step[2].count.host,
  instance = step[2].count.instance,
  continue = true,
}

-- Flapping status
step[18].count = {
  service = step[7].count.service,
  host = step[2].count.host,
  instance = step[2].count.instance,
  continue = true,
}

-- Host dependency
step[19].count = {
  host = step[2].count.host,
  instance = step[2].count.instance,
  continue = true,
}

-- Host parent
step[20].count = {
  host = step[2].count.host,
  instance = step[2].count.instance,
  continue = true,
}

-- Service dependency
step[21].count = {
  service = step[7].count.service,
  host = step[2].count.host,
  instance = step[2].count.instance,
  continue = true,
}

-- Instance status    => 16
step[22].count = {
  instance = step[1].count.instance,
  continue = true,
}

-- Logs               => 17
step[23].count = {
  log = 100,
  continue = true,
}

-- Ba status
step[24].count = {
  ba = 100,
  continue = true,
}

-- KPI status
step[25].count = {
  kpi = 100,
  continue = true,
}

-- Ba events
step[26].count = {
  ba = 100,
  continue = true,
}

-- KPI events
step[27].count = {
  kpi = 100,
  continue = true,
}

-- Table truncate signal
step[28].count = {
  continue = false,
}

local function round(val, decimal)
  if decimal then
    return math.floor( (val * 10^decimal) + 0.5) / (10^decimal)
  else
    return math.floor(val+0.5)
  end
end

function os.capture(cmd, raw)
  local f = assert(io.popen(cmd, 'r'))
  local s = assert(f:read('*a'))
  f:close()
  if raw then return s end
  s = string.gsub(s, '^%s+', '')
  s = string.gsub(s, '%s+$', '')
  s = string.gsub(s, '[\n\r]+', ' ')
  return s
end

function clean_tables()
  local queries = {
    -- We delete comments
    { "storage", "DELETE FROM comments" },
    -- This table can grow up quickly, so we clean it
    { "storage", "DELETE FROM data_bin" },
    { "storage", "DELETE FROM metrics" },
    -- We want hostgroups to be rebuilt entirely
    { "storage", "DELETE FROM hostgroups" },
    -- We delete custom variables
    { "storage", "DELETE FROM customvariables" },
    -- We want hostgroups without hosts associated
    { "storage", "INSERT INTO hostgroups (name) VALUES ('hostgroup_12')" },
    { "storage", "INSERT INTO hostgroups (name) VALUES ('hostgroup_13')" },
    { "storage", "INSERT INTO hostgroups (name) VALUES ('hostgroup_14')" },
    { "storage", "INSERT INTO hostgroups (name) VALUES ('hostgroup_15')" },
    { "cfg", "DELETE FROM mod_bam" },
    { "cfg", "DELETE FROM mod_bam_kpi" },
    { "storage", "DELETE FROM mod_bam_reporting_ba_events" },
    { "storage", "DELETE FROM mod_bam_reporting_kpi_events" },
    { "storage", "DELETE FROM hosts_hosts_dependencies" },
  }

  for _,l in ipairs(queries) do
    local cursor, error_str = simu.conn[l[1]]:execute(l[2])
    if error_str then
      error(error_str)
    end
  end
end

function init(conf)
  broker_log:info(0, "Start")
  for i,v in pairs(conf) do
    broker_log:info(0, tostring(i).." => " ..tostring(v))
  end
  broker_log:info(0, "Start end")
  math.randomseed(os.time())
  os.remove("/tmp/simu.log")
  broker_log:set_parameters(3, simu.log_file)
  local env = mysql.mysql()
  simu.conn = {}
broker_log:info(0, "login: "..tostring(conf['login']))
broker_log:info(0, "password: "..tostring(conf['password']))
broker_log:info(0, "db_addr: "..tostring(conf['db_addr']))
  simu.conn["storage"] = env:connect('centreon_storage', conf['login'], conf['password'], conf['db_addr'], 3306)
  if not simu.conn["storage"] then
    broker_log:error(0, "No connection to database")
    error("No connection to database")
  else
    simu.conn["storage"]:setautocommit(1)
    simu.conn["cfg"] = env:connect('centreon', conf['login'], conf['password'], conf['db_addr'], 3306)
    if not simu.conn["cfg"] then
      broker_log:error(0, "No connection to cfg database")
      error("No connection to cfg database")
    end
    simu.conn["cfg"]:setautocommit(1)

    -- Some clean up
    clean_tables()
    simu.start = os.clock()
    simu.previous_time = simu.start
  end
end

function read()
  if (simu.step_build == 1 or (simu.step_build > 1 and step[simu.step_build - 1].count.continue)) and #simu.stack == 0 then
    if simu.step_build == 1 then
      print(red .. "===== START =====" .. reset)
    end

    -- Building step in db
    if step[simu.step_build] then
      if step[simu.step_build].count.disabled then
        print(purple .. 'BUILD step ' .. simu.step_build .. ' disabled ' .. reset .. '(' .. step[simu.step_build].name .. ')')
        broker_log:info(0, 'Build Step ' .. simu.step_build .. ' disabled')
      else
        broker_log:info(0, "Build Step " .. simu.step_build)
        print(green .. "BUILD step " .. simu.step_build .. " " .. reset .. step[simu.step_build].name)
        step[simu.step_build].build(simu.stack, step[simu.step_build].count, simu.conn)
        print("   stack size " .. #simu.stack)
      end
      simu.step_build = simu.step_build + 1
    end
  end

  -- Check of step in db
  if simu.step_check < simu.step_build or not step[simu.step_check].count.continue then
    local cont = true
    if step[simu.step_check].count.disabled then
      print(purple .. "NO CHECK ON " .. reset .. step[simu.step_check].name)
      broker_log:info(0, "No check on " .. step[simu.step_check].name)
      simu.step_check = simu.step_check + 1
      broker_log:info(0, "Check Step " .. simu.step_check)
    else
      if step[simu.step_check].check(simu.conn, step[simu.step_check].count) then
        local now = os.clock()
        print(blue .. "CHECK " .. reset .. step[simu.step_check].name .. " DONE in " .. blue .. round(now - simu.previous_time, 3) .. 's' .. reset)
        simu.previous_time = now
        cont = step[simu.step_check].count.continue
        simu.step_check = simu.step_check + 1
        broker_log:info(0, "Check Step " .. simu.step_check)
      end
    end

    if not cont then
      broker_log:info(0, "No more step")
      simu.finish = os.clock()
      print(yellow .. "Execution duration: " .. reset .. (simu.finish - simu.start) .. "s")
      local output = os.capture("ps ax | grep \"\\<cbd\\>\" | grep -v grep | awk '{print $1}' ", 1)
      if output ~= "" then
        broker_log:info(0, "SEND COMMAND: kill " .. output)
        os.execute("kill -9 " .. output)
      end
    else
    end
  end

  -- Need to pop elemnts from the stack
  if #simu.stack > 0 then
    if #simu.stack % 100 == 0 then
      broker_log:info(0, "Stack contains " .. #simu.stack .. " elements")
    end
    broker_log:info(2, "EVENT SEND: " .. broker.json_encode(simu.stack[1]))
    return table.remove(simu.stack, 1)
  end
  return nil
end
