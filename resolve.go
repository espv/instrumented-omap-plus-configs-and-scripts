package main

import (
	"bufio"
	"flag"
	"fmt"
	// rbt "github.com/erriapo/redblacktree"
	"log"
	"os"
	"runtime/pprof"
	"strconv"
	"strings"
)

const (
	MAX_DIST int64 = 16384
)

var irqs map[string]string = map[string]string{
	"43":  "IRQ_PRCM",
	"44":  "IRQ_SDMA_0",
	"53":  "IRQ_GFX",
	"57":  "IRQ_DSS_DISPC",
	"69":  "IRQ_GPT1",
	"91":  "IRQ_MMC5",
	"93":  "IRQ_I2C4",
	"94":  "IRQ_AES2_S",
	"99":  "IRQ_HSI_P1",
	"102": "IRQ_UART4",
	"103": "IRQ_HSI_DMA",
	"115": "IRQ_MMC1",
	"124": "IRQ_HS_USB_MC_N",
	"125": "IRQ_HS_USB_DMA_N",
	"162": "IRQ_162",
	"177": "IRQ_177",
	"206": "IRQ_206",
	"282": "IRQ_282",
}

func lessInt64(o1, o2 interface{}) int {
	return int(o1.(int64) - o2.(int64))
}

var cpuprofile = flag.String("cpuprofile", "", "write cpu profile to file")

/*
	TODO: Better lookup algo (something trie-based)
	TODO: parens after function symbols
*/
func main() {
	// tree := rbt.NewTreeWith(lessInt64)

	flag.Parse()
	if *cpuprofile != "" {
		f, err := os.Create(*cpuprofile)
		if err != nil {
			log.Fatal(err)
		}
		fmt.Println("Running with profiling enabled")
		pprof.StartCPUProfile(f)
		defer pprof.StopCPUProfile()
	}

	f, _ := os.Open("./System.map")
	scan := bufio.NewScanner(f)

	symbolTable := make(map[int64]string)
	cache := make(map[int64]string)

	for scan.Scan() {
		s := scan.Text()
		l := strings.Split(s, " ")

		if d, err := strconv.ParseInt("0x"+l[0], 0, 64); err == nil {
			symbolTable[d] = l[2]
			// tree.Put(d, l[2])
		} else {
			fmt.Println("Unable to parse address")
			fmt.Println(err)
			return
		}
	}

	f.Close()

	f, _ = os.Open(os.Args[len(os.Args)-1])
	// o, _ := os.Open("trace.out.res")

	inputLines := make(chan string, 1000)

	go func() {
		scan = bufio.NewScanner(f)
		for scan.Scan() {
			inputLines <- scan.Text()
		}

		close(inputLines)
	}()

	modifiedLines := make(chan []string, 1000)

	go func() {
		for {
			line, ok := <-inputLines
			if !ok {
				break
			}

			l := strings.Split(line, " ")

			for i, word := range l {
				if len(word) != 8 {
					continue
				}

				var address int64
				var err error
				if address, err = strconv.ParseInt("0x"+word, 0, 64); err != nil {
					continue
				}

				var symbol string
				var offset int64 = 0
				/*
					if ok, v := tree.Get(address); !ok {
						for offset = 0; offset < MAX_DIST; offset++ {
							if ok, res := tree.Get(address - offset); ok {
								symbol = res.(string)
								// cache[address] = res
								tree.Put(address, res)
								break
							}
						}
					} else {
						symbol = v.(string)
					}
				*/

				if v, ok := cache[address]; ok {
					symbol = v
				} else {
					for offset = 0; offset < MAX_DIST; offset++ {
						if res, ok := symbolTable[address-offset]; ok {
							symbol = res
							// cache[address] = res
							break
						}
					}

					cache[address] = ""
				}

				if symbol != "" {
					if offset == 0 {
						l[i] = symbol
					} else {
						l[i] = symbol + "+" + strconv.FormatInt(offset, 16)
					}

					cache[address] = l[i]
				}
			}

			modifiedLines <- l
		}

		close(modifiedLines)
	}()

	done := make(chan bool)

	go func() {
		for {
			l, ok := <-modifiedLines
			if !ok {
				break
			}
			widths := []string{"13", "3", "6", "13", "25", "6", "9", "30", "30"}
			field := 0
			for _, s := range l {
				if s == "" {
					continue
				}

				fmt.Printf("%-"+widths[field]+"s", s)

				field += 1
			}
			fmt.Println()
		}

		done <- true
	}()

	<-done
}
