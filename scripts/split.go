package main

import (
	"bufio"
	"fmt"
	"os"
	"os/exec"
	"regexp"
)

var (
	e = regexp.MustCompile("^([0-9\\-]+\\s+[0-9:]+)\\.\\d+ (\\d+)\\s+Len=(\\d+)")
)

type Event struct {
	time   string
	port   string
	length string
}

func (e *Event) isStart() bool {
	return e.length == "6"
}

func (e *Event) isEnd() bool {
	return e.length == "4"
}

func extract(ch chan *Event, done chan bool) {
	var start *Event = nil
	var end *Event = nil

	n := 0

	for ev, ok := <-ch; ok; ev, ok = <-ch {
		if ev.isStart() {
			start = ev
		} else if ev.isEnd() && start != nil {
			end = ev

			fmt.Printf("Segment %d: %s -> %s\n", n, start.time, end.time)

			cmd := exec.Command("editcap", "-A", start.time, "-B", end.time, os.Args[1], fmt.Sprintf("exp%d.pcap", n))

			if output, err := cmd.Output(); err != nil {
				fmt.Println(err)
				fmt.Println(output)
				os.Exit(-1)
			}

			start, end = nil, nil
			n++
		}
	}

	done <- true
}

func main() {
	if len(os.Args) < 2 {
		fmt.Println("Usage: ./split <pcap file>")
		return
	}
	var in *bufio.Scanner
	if f, err := os.Open("segments"); err != nil {
		fmt.Println(err)
	} else {
		in = bufio.NewScanner(f)
	}

	ch := make(chan *Event)
	done := make(chan bool)

	go extract(ch, done)

	for in.Scan() {
		str := in.Text()

		if sub := e.FindStringSubmatch(str); sub != nil {
			fmt.Printf("%+v\n", sub[1:])
			ch <- &Event{sub[1], sub[2], sub[3]}
		}
	}

	close(ch)
	<-done
}
