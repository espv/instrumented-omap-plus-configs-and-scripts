package main

import (
	"bufio"
	"fmt"
	"os"
	"regexp"
	"strconv"
)

func delta(a, b int) (ret int) {
	if b < a {
		// 32-bits assumption
		ret = (0xFFFFFFFF - a) + b
		// fmt.Println("WRAP")
	} else {
		ret = b - a
	}

	return
}

type Recv struct {
	time int64
}

type Send struct {
	time int64
}

var whitespace = regexp.MustCompile("\\s+")

func main() {
	f, err := os.Open(os.Args[1])
	if err != nil {
		fmt.Println(err)
		return
	}

	scan := bufio.NewScanner(f)
	scan.Split(bufio.ScanLines)

	rxstart := int64(-1)
	// totalcyc := int64(0)
	lastCycles := 0
	cycles := int64(0)

	packets := make(map[int64]int64)

	for scan.Scan() {
		line := scan.Text()

		entry := whitespace.Split(line, -1)

		if entry[0] == "EOD" {
			packets = make(map[int64]int64)
			rxstart = -1
			fmt.Println("EOD")
			continue
		}

		if len(entry) < 3 {
			continue
		}

		if entry[1] != "0" {
			continue
		}

		cyc, err := strconv.Atoi(entry[3])

		if err != nil {
			fmt.Println(err)
			continue
		}

		d := delta(lastCycles, cyc)

		lastCycles = cyc

		cycles += int64(d)

		// fmt.Println(cyc, cycles)

		if entry[0] != "PKTEXTR" {
			continue
		}

		fmt.Println(entry)

		eventType, err := strconv.ParseInt(entry[5], 10, 64)
		if err != nil {
			fmt.Println(err)
			continue
		}

		seqno, err := strconv.ParseInt(entry[6], 16, 64)
		if err != nil {
			fmt.Println(err)
			continue
		}

		fmt.Println(eventType, seqno)

		switch eventType {
		case 0:
			rxstart = cycles
			break
		case 1:
			if rxstart == -1 {
				break
			}

			if seqno != 0 {
				fmt.Printf("Received %d\n", seqno)
				// packets[seqno] = rxstart
				packets[seqno] = cycles
			}
			rxstart = -1
			break
		case 2:
			if seqno != 0 {
				fmt.Printf("Sent %d\n", seqno)

				if v, ok := packets[seqno]; ok {
					fmt.Printf("DELTA(%d): %d\n", seqno, cycles-v)
					delete(packets, seqno)
				}
			}
			break
		case 4:
			break
		default:
			panic(fmt.Sprint("Got unknown event type ", eventType))
			break
		}

		/*
			if eventType == 4 { // Right before RX loop
				// fmt.Println("New RXLOOP", cycles)
				// rxstart = cycles
			} else if eventType == 0 { // Start or restart of driver receive loop
				rxstart = cycles
				fmt.Println("RXLOOP (re)start:", rxstart)
			} else if eventType == 1 { // Packet sent to IP layer
				packets[addr] = append(packets[addr], Recv{rxstart})

				fmt.Printf("Received packet 0x%x\n", addr)

				// packets[addr] = []int64{rxstart}

				// rxstart = cycles
			} else if eventType == 2 { // Packet sent by driver

				// Adjusting the addr for L2 header size
				// addr -= 30

				fmt.Printf("Sent packet 0x%x\n", addr)

				if len(packets[addr]) == 1 {
					packets[addr] = append(packets[addr], Send{cycles})

					// fmt.Printf("Delta: %v\n", packets[addr][1]-packets[addr][0])

					send, sendOk := packets[addr][1].(Send)
					recv, recvOk := packets[addr][0].(Recv)

					if sendOk && recvOk {
						fmt.Printf("Delta: %v\n", send.time-recv.time)
					}

					// delete(packets, addr)
				}

				// This address is no longer of interest.
				// Either the packet was forwarded, or we didn't capture the arrival
				// of this packet. The buffer address may be reused in either case.
				delete(packets, addr)

			} else {
				fmt.Println(eventType)
				panic("Unknown event type")
			}
		*/
	}

	fmt.Println("done reading")

	/*
		for _, packet := range packets {
			if len(packet) > 2 {
				// fmt.Printf("DISCARDED %v\n", a)
			} else if len(packet) < 2 {
				// fmt.Printf("DROPPED %v\n", a)
			} else {
				fmt.Printf("%v\n", float64(packet[1]-packet[0])/float64(1200000000))
			}
		}
	*/
}
