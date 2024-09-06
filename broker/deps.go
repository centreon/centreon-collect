package main

import (
    "bufio"
    "fmt"
    "log"
    "os"
    "path/filepath"
    "regexp"
    "strings"
    )

// MaxDepth Max depth in search tree
const MaxDepth = 3

func findIncludes(file string, treated *[]string, depth int, outputs map[string]bool) {
  var myList []string
  file1 := filepath.Clean(file)
    f, err := os.Open(file1)
    if err != nil {
      file1 = strings.TrimPrefix(file, "inc/")
      for _, pref := range []string{
                              "/usr/local/include/",
                              "rrd/inc/",
                              "generator/inc/",
                              "graphite/inc/",
                              "tls/inc/",
                              "lua/inc/",
                              "redis/inc/",
                              "neb/inc/",
                              "tcp/inc/",
                              "bam/inc/",
                              "core/inc/",
                              "watchdog/inc/",
                              "stats/inc/",
                              "notification/inc/",
                              "../bbdo/",
                              "dumper/inc/",
                              "storage/inc/",
                              "unified_sql/inc/",
                              "influxdb/inc/",
                              "sql/inc/" } {
        f, err = os.Open(pref + file1)
        if err == nil {
          file1 = pref + file1
          *treated = append(*treated, file1)
          break
        }
      }
    } else {
      *treated = append(*treated, file1)
    }
    defer f.Close()

  depth++
  if depth > MaxDepth {
    return
  }

  scanner := bufio.NewScanner(f)
  r, _ := regexp.Compile("^#\\s*include\\s*([<\"])(.*)[>\"]")

  for scanner.Scan() {
    line := scanner.Text()
    match := r.FindStringSubmatch(line)
    if len(match) > 0 {
    /* match[0] is the global match, match[1] is '<' or '"' and match[2] is the file to include */
      var output string
      if depth == 1 {
	if match[1] == "\"" {
	  output = fmt.Sprintf("  \"%s\" -> \"%s\" [color=blue];\n", file, match[2])
	  myList = append(myList, match[2])
	} else {
	  output = fmt.Sprintf("  \"%s\" -> \"%s\" [color=blue];\n", file, match[2])
	}
      } else {
	if match[1] == "\"" {
	  output = fmt.Sprintf("  \"%s\" -> \"%s\";\n", file, match[2])
	  myList = append(myList, match[2])
	} else {
	  output = fmt.Sprintf("  \"%s\" -> \"%s\";\n", file, match[2])
	}
      }
      outputs[output] = true
    }
  }
  if err := scanner.Err(); err != nil {
    log.Print(file, " --- ", err)
  }

  for _, file2 := range myList {
    found := false
    for _, ff := range *treated {
      if ff == file2 {
        found = true
        break
      }
    }
    if !found {
      findIncludes(file2, treated, depth, outputs)
    }
  }
}

func main() {
  args := os.Args[1:]
  var fileList []string

  if len(args) == 0 {
    for _, searchDir := range []string{"src", "inc"} {
      filepath.Walk(searchDir, func(path string, f os.FileInfo, err error) error {
        if strings.HasSuffix(path, ".cc") || strings.HasSuffix(path, ".hh") {
          fileList = append(fileList, path)
        }
        return nil
      })
    }
  } else {
    fileList = append(fileList, args[0])
  }

  fmt.Println("digraph deps {")

  var treated []string
  outputs := make(map[string]bool)
  for _, file := range fileList {
    findIncludes(file, &treated, 0, outputs)
  }
  for output := range outputs {
    fmt.Println(output)
  }
  fmt.Println("}")
}
