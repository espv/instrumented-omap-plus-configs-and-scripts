package main

import (
	"bufio"
	"fmt"
	"io"
	"os"
	"regexp"
	"strings"
)

var (
	colDescripor = regexp.MustCompile("^\\| Col (\\d+): (.*) \\|$")
	tableSep     = regexp.MustCompile("^=*$")
	preambleSep  = regexp.MustCompile("^\\|-*\\|$")
	headerSep    = regexp.MustCompile("^\\|-+\\|.*$")
)

func readFile(name string, c chan string) {
	var in *bufio.Scanner
	if f, err := os.Open(name); err != nil {
		fmt.Println(err)
		return
	} else {
		in = bufio.NewScanner(f)
	}

	in.Split(bufio.ScanLines)
	for in.Scan() {
		c <- in.Text()
	}

	close(c)
}

var parse func(chan string, chan string) bool

func parsePreamble(in, out chan string) bool {
	for str, ok := <-in; ok; str, ok = <-in {

		out <- "# " + str

		if colDescripor.MatchString(str) {
			// fmt.Println("COL")
			// Nothing being done with column descriptors for now
		} else if preambleSep.MatchString(str) {
			parse = parseHeader
			return true
		}
	}

	close(out)
	return false
}

func parseHeader(in, out chan string) bool {
	for str, ok := <-in; ok; str, ok = <-in {
		if headerSep.MatchString(str) {
			parse = parseResultLine
			return true
		}
	}

	close(out)
	return false
}

func parseResultLine(in, out chan string) bool {
	for str, ok := <-in; ok; str, ok = <-in {
		if tableSep.MatchString(str) {
			break
		}
		fields := strings.Split(str, "|")

		outStr := ""
		for _, field := range fields {
			field = strings.Trim(field, " \t")
			if len(field) == 0 {
				continue
			}

			if outStr != "" {
				outStr += ", "
			}
			outStr += field
		}

		out <- outStr
	}

	close(out)
	return false
}

func write(out chan string, output io.Writer) {
	for str, ok := <-out; ok; str, ok = <-out {
		// output.Write([]byte(str))
		fmt.Println(str)
	}
}

func main() {
	if len(os.Args) < 2 {
		fmt.Println("Usage: stats_parse <stats file>")
		return
	}

	in, out := make(chan string), make(chan string)

	parse = parsePreamble

	go readFile(os.Args[1], in)
	go write(out, os.Stdout)

	for parse(in, out) {
	}
}
