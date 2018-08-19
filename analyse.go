package main

/*
TODO:
Avoid sending huge data structutes from function to function, use channels
Store event types as integers for fast comparisons
Directory structures and file names + utility functions
Command line options
Paralellism
halt() function for handling trace file errors
*/

import (
	"bufio"
	"fmt"
	"math"
	"os"
	"os/exec"
	"regexp"
	"sort"
	"strconv"
	"strings"
)

const (
	ENQUEUE = 0
	DEQUEUE = 1
)

var (
	pktextrnr int = 0

	softirqs = map[string]string{
		"1": "run_timer_softirq",
		"2": "net_tx_action",
		"3": "net_rx_action",
	}

	state_ids = map[int]string{
		0:  "definedindevicefile",
		1:  "kernel:workpending",
		2:  "kernel:reschedqdisk",
		3:  "kernel:stopqdisk",
		4:  "wl1251:readinterrupttype",
		5:  "wl1251:interrupttype",
		6:  "wl1251:interruptenabled",
		7:  "wl1251:numtxcomplete",
		8:  "kernel:ifforxmit",
		9:  "wl1251:sizeofnextrxfromnic",
		10: "wl1251:triggernictx",
		11: "bcmdhd:singleread",
		12: "bcmdhd:dataavailable",
		13: "bcmdhd:ctrlframe",
	}

	synch_ids = map[int]string{
		0: "temporary::completion",
		1: "bcm4329::dpc",
		2: "wl12xx:mcspirx",
		3: "wl12xx:mcspitx",
	}

	state_operations = map[int]string{
		0: "read",
		1: "write",
	}

	state_scopes = map[int]string{
		0: "local",
		1: "global",
	}

	addr_map = map[string]int{}

	addr_map_synch = map[string]int{}

	queue_ids = map[int]string{
		0:  "noloc",
		1:  "ip::backlog",
		2:  "bcm4329::driver::tx",
		3:  "wl1251::driver::tx",
		4:  "nic::rx",
		5:  "nic::tx",
		6:  "nic::Q",
		7:  "dma::readycmd1",
		8:  "dma::readycmd2",
		9:  "dma::readycmd3",
		10: "dma::readycmd4",
		11: "dma::readycmd5",
		12: "dma::readycmd6",
		13: "dma::readycmd7",
		14: "dma::readycmd8",
		15: "dma::readycmd9",
		16: "dma::readycmd10",
		17: "dma::readycmd11",
		18: "dma::readycmd12",
		19: "dma::readycmd13",
		20: "dma::readycmd14",
		21: "dma::readycmd15",
		22: "dma::readycmd16",
		23: "qdisc",
		24: "SEPARATOR",
		25: "softirq::hi",
		26: "softirq::timer",
		27: "softirq::tx",
		28: "softirq::rx",
		29: "softirq::block",
		30: "softirq::block-iopoll",
		31: "softirq::tasklet",
		32: "softirq::sched",
		33: "softirq::hrtimer",
		34: "tasklet::scheduled",
		35: "tasklet::scheduled-hi",
		36: "hrtimer::scheduled",
		37: "omap2mcspi",
		38: "wl1251:hwwork",
		39: "HIRQ",
		40: "SEPARATOR",
		41: "dma::spisizes",
		42: "dma::spioperations",
	}
)

type EventHandleFunc func(*State, *Entry)

// Entry corresponds to each line in the trace file
type Entry struct {
	EventType  string
	CPU        int
	CurrentPid int
	Pid1       int
	Pid2       int
	PacketId   string
	Service    string
	Location   string
	Cycles     int
	// Line        string
	LineNr      int
	TempVarId   int
	TempVars    []TempVarEntry
	PEUDuration int
	PEUIRQ      int
}

type OngoingPEU struct {
	Start   *Entry
	Entries []*Entry
}

// ServiceStackElement is the type for each element in the service stack
type ServiceStackElement struct {
	Name      string
	Entries   []*Entry
	PacketIds []int
}

// State contains the current state of the analysis
type State struct {
	// Per-CPU LEUs and service stack
	CurrentLEU   map[int]LEU
	ServiceStack map[int]map[LEU][]ServiceStackElement

	// CurrentLEU   LEU
	// ServiceStack map[LEU][]ServiceStackElement

	Interrupted []LEU
	CurrentPEU  OngoingPEU

	CurrentPEUs []OngoingPEU

	LEUs map[LEU]map[string][]LEUentry

	TempVars     map[string]*TempVar
	LastAppended *Entry
	TempVarId    int
}

// GetCurrentLEU returns the LEU currently running on the specified CPU
func (s *State) GetCurrentLEU(entry *Entry) LEU {
	return s.CurrentLEU[entry.CPU]
}

// GetServiceStack gets the service stack for the specified CPU
func (s *State) GetCurrentServiceStack(entry *Entry) []ServiceStackElement {
	if m, ok := s.ServiceStack[entry.CPU]; ok {
		return m[s.GetCurrentLEU(entry)]
	} else {
		// Create a map for the CPU if it doesn't exist already
		s.ServiceStack[entry.CPU] = make(map[LEU][]ServiceStackElement)
		return s.ServiceStack[entry.CPU][s.GetCurrentLEU(entry)]
	}
}

type LEUentry struct {
	Entries   []*Entry
	PacketIds []int
}

type LEU struct {
	Name string
	Pid  int
}

type Fraction struct {
	Percent float64
	Count   int
	Total   int
	SignatureCollection
}

type FractionSlice []Fraction

func (f FractionSlice) Len() int           { return len(f) }
func (f FractionSlice) Swap(i, j int)      { f[i], f[j] = f[j], f[i] }
func (f FractionSlice) Less(i, j int) bool { return f[i].Percent > f[j].Percent } // Reverse order

// TODO: Sanity
type Profile map[string]map[string]*SignatureCollection

type Signature struct {
	Events   []string
	Duration []int
}

type SignatureCollection struct {
	Events    []string
	Durations [][]int
}

type TempVar struct {
	Entry          *Entry
	Value          int
	TempVarEntries []TempVarEntry
}

type TempVarEntry struct {
	Name  string
	Value int
}

// rjust right aligns a string if its length is less that the width parameter
func rjust(str string, width int) string {
	if padding := width - len(str); padding > 0 {
		return strings.Repeat(" ", padding) + str
	}
	return str
}

// ljust left aligns a string if its length is less that the width parameter
func ljust(str string, width int) string {
	if padding := width - len(str); padding > 0 {
		return str + strings.Repeat(" ", padding)
	}
	return str
}

func (e Entry) GetIRQ() int {
	if len(e.EventType) < 4 || e.EventType[0:4] != "HIRQ" {
		panic("ERROR: IRQ number requested from a non-IRQ entry")
	}

	return e.Pid1
}

func (e Entry) GetQueueAction() int {
	if e.EventType != "SRVQUEUE" {
		panic("ERROR: GetQueueAction() called on incorrect entry type")
	}

	return e.Pid1
}

func (e Entry) GetQueueCond() string {
	if e.PacketId == "0" {
		return "notempty"
	} else {
		return "empty"
	}
}

func GetStateId(id int) string {
	if state_id, ok := state_ids[id]; ok {
		return state_id
	} else {
		panic("GetStateId()")
	}
}

func GetStateIdString(str string) string {
	id, err := strconv.ParseInt(str, 16, 64)
	if err != nil {
		panic("GetStateIdString()")
	}
	return GetStateId(int(id))
}

func (e Entry) GetStateOperation() string {
	return state_operations[e.Pid1]
}

func GetStateScopeStr(str string) string {
	id, err := strconv.Atoi(str)
	if scope, ok := state_scopes[id]; err == nil && ok {
		return scope
	}
	return "UNKNOWN-SCOPE"
}

func GetStateScope(id int) string {
	if scope, ok := state_scopes[id]; ok {
		return scope
	}
	return "UNKNOWN-SCOPE"
}

func GetQueueId(id int) string {
	if queue, ok := queue_ids[id]; ok {
		return queue
	}

	return "UNKNOWN-QID"
}

func GetQueueIdByString(str string) string {
	id, err := strconv.ParseInt(str, 16, 64)
	if err != nil {
		return "UNKNOWN-QID"
	}
	if queue, ok := queue_ids[int(id)]; ok {
		return queue
	} else {
		return "UNKNOWN-QID"
	}
}

func (state State) Pop(entry *Entry) ServiceStackElement {
	stack := state.GetCurrentServiceStack(entry)
	service := stack[len(stack)-1]
	state.ServiceStack[entry.CPU][state.CurrentLEU[entry.CPU]] = stack[:len(stack)-1]
	return service
}

func (s State) String() (str string) {
	str += fmt.Sprint(s.CurrentLEU)
	str += "\n"
	// str += fmt.Sprint("Current stack: ", s.ServiceStack[s.CurrentLEU])
	str += "\n"

	return
}

func (s *State) GetTempVarId() (retval int) {
	retval = s.TempVarId
	s.TempVarId += 1
	return
}

func (l LEUentry) String() string {
	return fmt.Sprintf("%d %s", l.PacketIds, l.Entries)
}

func (se ServiceStackElement) String() string {
	s := se.Name + "\n"

	for _, entry := range se.Entries {
		s += fmt.Sprintf("\t%s %d %s %d\n", entry.EventType, entry.Cycles, entry.Location, entry.LineNr)
	}

	return s
}

func (l LEU) String() (str string) {
	str = fmt.Sprintf("LEU(%s, %d)", l.Name, l.Pid)
	return
}

var (
	// Various regular expressions used during the trace
	comment    = regexp.MustCompile("^#.*")
	emptyLine  = regexp.MustCompile("^\\s*$")
	whitespace = regexp.MustCompile("\\s+")

	// This hashmap determines which function to call based on the event
	//	encountered in the trace
	handleEvent = map[string]EventHandleFunc{
		"CTXSW":      handleCTXSW,
		"MIGRATE":    handleMIGRATE,
		"HIRQENTRY":  handleHIRQEntry,
		"HIRQEXIT":   handleHIRQExit,
		"LOOPRSTART": handleLOOPRSTART,
		"LOOPSTART":  handleLOOPSTART,
		"LOOPSTOP":   handleLOOPSTOP,
		"PEUSTART":   handlePEUSTART,

		"COMPL":     handleSync,
		"SEMDOWN":   handleSync,
		"SEMUP":     handleSync,
		"WAITCOMPL": handleSync,

		"PKTQUEUE":  handlePKTQUEUE,
		"PKTQUEUEN": handlePKTQUEUE,

		"SRVENTRY":  handleSRVENTRY,
		"SRVEXIT":   func(s *State, e *Entry) { handleSRVEXIT(s, e) },
		"SRVQUEUE":  handleSRVQUEUE,
		"TEMPSYNCH": handleTEMPSYNCH,

		"QUEUECOND": func(s *State, e *Entry) { s.PushService(e) },
		"STATECOND": func(s *State, e *Entry) { s.PushService(e) },

		"STATEQUEUE": func(s *State, e *Entry) { s.PushService(e) },
		"SLEEP":      func(s *State, e *Entry) { s.PushService(e) },
		"TTWAKEUP":   func(s *State, e *Entry) { s.PushService(e) },
		"WAKEUP":     func(s *State, e *Entry) { s.PushService(e) },
	}
)

var leuNames map[int]string

var longestLocationString int = -1

func main() {
	state := State{}
	run(os.Args[1], &state)
	storeCases(&state)
	createSignatures(&state)
}

// Each CPU has its own cycle counter, so they must be handled separately.
type PerCPUTraceState struct {
	first     bool
	limbo     bool
	lastCycle int
	cycles    int
	cyc       int // contains the last cycle count we read from the trace
}

func run(filename string, state *State) (err error) {
	f, _ := os.Open(filename)
	defer func() { err = f.Close() }()

	scanner := bufio.NewScanner(f)
	scanner.Split(bufio.ScanLines)

	traceState := make([]PerCPUTraceState, 2)
	for i := range traceState {
		traceState[i].first = true
		traceState[i].limbo = true
		traceState[i].lastCycle = 0
		traceState[i].cycles = 0
	}

	state.LEUs = make(map[LEU]map[string][]LEUentry)
	state.ServiceStack = make(map[int]map[LEU][]ServiceStackElement)
	state.CurrentLEU = make(map[int]LEU)
	state.TempVars = make(map[string]*TempVar)

	cmdOutput, err := exec.Command("wc", "-l", os.Args[1]).Output()
	if err != nil {
		fmt.Println("Unable to find number of lines in input file")
		fmt.Println(err)
		os.Exit(-1)
	}

	totalLineNumbers, err := strconv.Atoi(strings.Split(string(cmdOutput), " ")[0])
	if err != nil {
		fmt.Println("Unable to find number of lines in input file")
		fmt.Println(err)
		os.Exit(-1)
	}

	iteration := 0
	lineNumber := 0
	for scanner.Scan() {
		lineNumber += 1
		s := scanner.Text()

		percCompleted := float64(lineNumber) / float64(totalLineNumbers) * 100
		progress := fmt.Sprintf("%3d%% (%8d/%8d)", int(percCompleted), int(lineNumber), int(totalLineNumbers))
		fmt.Printf("\r%s", progress)

		// Reset on End of Data
		if s[0:3] == "EOD" {
			iteration += 1
			// fmt.Println("EOD")

			for i := range traceState {
				traceState[i].first = true
				traceState[i].limbo = true
			}

			state.ServiceStack = make(map[int]map[LEU][]ServiceStackElement)
			state.CurrentLEU = make(map[int]LEU)
			state.TempVars = make(map[string]*TempVar)
			state.CurrentPEU = OngoingPEU{nil, nil}
			state.CurrentPEUs = nil

			continue
		}

		entry := whitespace.Split(s, -1)

		if entry[0] == "ID_QUEUE" && entry[7] != "0" {
			var pid1 int
			if pid1, err = strconv.Atoi(entry[4]); err != nil {
				panic(err)
				return
			}

			addr_map[entry[7]] = pid1
			continue
		}

		if entry[0] == "ID_SYNCH" && entry[7] != "0" {
			var pid1 int
			if pid1, err = strconv.Atoi(entry[4]); err != nil {
				panic(err)
				return
			}

			addr_map_synch[entry[7]] = pid1
			continue
		}

		// Handle cutoffs (eg. sigkill on usnl)
		if len(entry) < 9 {
			continue
		}

		var currentCPU int
		if currentCPU, err = strconv.Atoi(entry[1]); err != nil {
			panic(err)
			return
		}

		// Load CPU-specific trace state to manage cycle counts.
		ts := &traceState[currentCPU]

		// Handle limbo's
		//
		// After EOD, we don't know the current context once the trace continues.
		// Once we encounter a CTXSW, we know we're in a process context and not
		// inside an HIRQ leu.
		if ts.limbo {
			if entry[0] == "CTXSW" {
				ts.limbo = false
			} else {
				continue
			}
		}

		var currentPid int
		if currentPid, err = strconv.Atoi(entry[2]); err != nil {
			return
		}

		delta := func(a, b int) (ret int) {
			if b < a {
				// 32-bits assumption
				ret = (0xFFFFFFFF - a) + b
			} else {
				ret = b - a
			}

			return
		}

		ts.lastCycle = ts.cyc

		if ts.cyc, err = strconv.Atoi(entry[3]); err != nil {
			panic(err)
			return
		}

		if ts.first {
			ts.first = false
			ts.lastCycle = ts.cyc
		}

		last := ts.cycles
		ts.cycles = ts.cycles + delta(ts.lastCycle, ts.cyc)
		ts.lastCycle = last

		var pid1 int
		if pid1, err = strconv.Atoi(entry[4]); err != nil {
			panic(err)
			return
		}

		var pid2 int
		if pid2, err = strconv.Atoi(entry[5]); err != nil {
			panic(err)
			return
		}

		e := Entry{
			EventType:  entry[0],
			CPU:        currentCPU,
			CurrentPid: currentPid,
			Pid1:       pid1,
			Pid2:       pid2,
			PacketId:   entry[6],
			Service:    entry[7],
			Location:   entry[8],
			Cycles:     ts.cycles,
			LineNr:     lineNumber,
			// Line:       strings.Join(entry, " "),
		}

		if length := len(e.Location); length > longestLocationString {
			longestLocationString = length
		}

		if handleFunc, ok := handleEvent[e.EventType]; ok {
			handleFunc(state, &e)
		} else {
			// panic(fmt.Sprintf("Unhandled event: %s\n", e.EventType))
		}
	}

	return
}

// Push the given entry to the current LEU service stack
func (s *State) PushEntry(entry *Entry) {
	stack := s.GetCurrentServiceStack(entry)
	serviceCount := len(stack)
	stack[serviceCount-1].Entries =
		append(stack[serviceCount-1].Entries, entry)

	count := len(stack[serviceCount-1].Entries)
	s.LastAppended = stack[serviceCount-1].Entries[count-1]
}

// Push a new service onto the current LEU service stack
func (s *State) PushNewService(entry *Entry, newService string) int {
	serviceCount := len(s.GetCurrentServiceStack(entry))

	// Register the call in the current service if there is one
	if serviceCount > 0 && !strings.Contains(entry.Location, "irq_exit") {
		e := *entry
		s.PushEntry(&e)
	}

	// Push the new service onto the stack
	stack := s.GetCurrentServiceStack(entry)
	s.ServiceStack[entry.CPU][s.GetCurrentLEU(entry)] = append(stack, ServiceStackElement{newService, []*Entry{}, []int{}})

	// We need to make a copy here so that the LOOP* stuff doesn't accidentally
	// update more than one entry
	entryCopy := *entry
	return s.PushService(&entryCopy)
}

// Push a service onto the current LEU service stack
func (s *State) PushService(entry *Entry) int {
	serviceCount := len(s.GetCurrentServiceStack(entry))

	if serviceCount > 0 {
		// fmt.Println("APPEND ", s.ServiceStack[s.CurrentLEU][len(s.ServiceStack[s.CurrentLEU])-1].Name)
		// fmt.Println("APPEND", s.ServiceStack[s.CurrentLEU][len(s.ServiceStack[s.CurrentLEU])-1].Name, entry)
		// fmt.Println()
		s.PushEntry(entry)
	}

	s.LastAppended = entry

	return serviceCount
}

// AppendToPEU handles events that may be coupled with a PEUSTART event
func (s *State) AppendToPEU(entry *Entry) {
	if entry.CPU == 1 {
		// TODO: Change this to handle SMP

		// fmt.Println("Ignored PEU-related event: ", entry.Line)
		// panic("Current code does not support PEU events not handled by cpu 0")
		return
	}

	if entry.EventType == "PEUSTART" {
		s.CurrentPEU = OngoingPEU{entry, []*Entry{}}
	} else {
		if s.CurrentPEU.Start == nil {
			return
		}
		s.CurrentPEU.Entries = append(s.CurrentPEU.Entries, entry)
	}

	interruptWithIndex := 0
	synchvar := ""
	intwithcompl := 0

	for index, entry := range s.CurrentPEU.Entries {
		if entry.EventType == "HIRQENTRY" {
			interruptWithIndex = index
		} else if entry.EventType == "HIRQEXIT" {
			interruptWithIndex = 0
		} else if len(entry.EventType) >= 5 && entry.EventType[0:5] == "COMPL" {
			if interruptWithIndex != 0 {
				intwithcompl = interruptWithIndex
			}

			if synchvar == "" {
				synchvar = entry.Service
			} else if synchvar == entry.Service && intwithcompl != 0 {
				// Got a match
				duration := s.CurrentPEU.Entries[intwithcompl].Cycles - s.CurrentPEU.Start.Cycles
				irq := s.CurrentPEU.Entries[intwithcompl].Pid1
				s.CurrentPEU.Start.PEUDuration = duration
				s.CurrentPEU.Start.PEUIRQ = irq

				s.CurrentPEU = OngoingPEU{nil, []*Entry{}}
			}
		} else if len(entry.EventType) >= 9 && entry.EventType[0:9] == "WAITCOMPL" {
			if synchvar == "" {
				synchvar = entry.Service
			} else if synchvar == entry.Service && intwithcompl != 0 {
				// Got a match
				duration := s.CurrentPEU.Entries[intwithcompl].Cycles - s.CurrentPEU.Start.Cycles
				irq := s.CurrentPEU.Entries[intwithcompl].Pid1
				s.CurrentPEU.Start.PEUDuration = duration
				s.CurrentPEU.Start.PEUIRQ = irq

				s.CurrentPEU = OngoingPEU{nil, []*Entry{}}
			}
		}
	}
}

func handleCTXSW(state *State, entry *Entry) {
	state.PushService(entry)                                // Append to previous service
	state.CurrentLEU[entry.CPU] = LEU{"Thread", entry.Pid2} // Switch the current LEU

	state.PushService(entry) // Append to next service

	// Create a new LEU entry if it doesn't exist
	if _, ok := state.LEUs[state.CurrentLEU[entry.CPU]]; !ok {
		state.LEUs[state.CurrentLEU[entry.CPU]] = make(map[string][]LEUentry)
	}
}

func handleMIGRATE(state *State, entry *Entry) {

	/*
		This code is tied up to how the mainline Linux scheduler works.
		It assumes separate run queues for each CPU.

		If the LEU is already on the target CPU stack, then it suggests a bug in
		the tracing code (or the scheduler).

		This assumes that a LEU will not be present on more than one stack.
		If the LEU is present in more than one CPU stack, then something weird has happened.
	*/

	newCPU := entry.Pid2
	migratedLEU := LEU{"Thread", entry.Pid1}
	for cpu, _ := range state.ServiceStack {
		if cpu == newCPU {
			continue
		}

		if stack, ok := state.ServiceStack[cpu][migratedLEU]; ok {
			// fmt.Println("MIGRATE", entry.Pid1, "TO", entry.Pid2)

			state.ServiceStack[newCPU][migratedLEU] = stack
			delete(state.ServiceStack[cpu], migratedLEU)

			break
		}
	}

	// Check if a LEU for the given thread exists

	// Check the service stack on each CPU
	// Move the LEU to the new service stack

	// Context switch will create LEU entry later if needed
}

func handleSRVENTRY(state *State, entry *Entry) {
	entry.PacketId = "0"
	state.PushNewService(entry, entry.Service)
}

func (s *State) PushServiceToLEU(entry *Entry, stackElem ServiceStackElement) {
	// fmt.Printf("Pushed service to %s: %s %v\n", s.CurrentLEU, stackElem.Name, stackElem.Entries)
	// fmt.Println(s.CurrentLEU[entry.CPU], stackElem.Name)
	// fmt.Println(*entry)

	currentLEU := s.CurrentLEU[entry.CPU]
	s.LEUs[currentLEU][stackElem.Name] =
		append(s.LEUs[currentLEU][stackElem.Name],
			LEUentry{stackElem.Entries, stackElem.PacketIds})
}

// This function retuns a bool in order to handle HIRQExit properly later
func handleSRVEXIT(state *State, entry *Entry) bool {
	if state.PushService(entry) == 0 {
		return false
	}

	// Pop last service
	service := state.Pop(entry)

	if _, ok := state.LEUs[state.CurrentLEU[entry.CPU]]; !ok {
		state.LEUs[state.CurrentLEU[entry.CPU]] = make(map[string][]LEUentry)
	}

	// Create service entry in LEU if it doesn't exist
	if _, ok := state.LEUs[state.CurrentLEU[entry.CPU]][service.Name]; !ok {
		state.LEUs[state.CurrentLEU[entry.CPU]][service.Name] = make([]LEUentry, 0)
	}

	state.PushServiceToLEU(entry, service)
	state.PushService(entry)

	return true
}

func handleHIRQEntry(state *State, entry *Entry) {
	state.PushService(entry)

	state.Interrupted = append(state.Interrupted, state.GetCurrentLEU(entry))

	state.CurrentLEU[entry.CPU] = LEU{"Hardware Interrupt", entry.GetIRQ()}

	if _, ok := state.LEUs[state.CurrentLEU[entry.CPU]]; !ok {
		state.LEUs[state.CurrentLEU[entry.CPU]] = make(map[string][]LEUentry)
	}

	irqService := fmt.Sprintf("HIRQ-%d", entry.GetIRQ())
	if serviceCount := state.PushNewService(entry, irqService); serviceCount > 0 {
		entries := state.GetCurrentServiceStack(entry)[serviceCount-1].Entries
		last := entries[len(entries)-1]
		state.AppendToPEU(last)
	}
}

func handleHIRQExit(state *State, entry *Entry) {
	if !handleSRVEXIT(state, entry) {
		return
	}

	// Pop from interrupted, go back to last service before IRQ
	state.CurrentLEU[entry.CPU] = state.Interrupted[len(state.Interrupted)-1]
	state.Interrupted = state.Interrupted[:len(state.Interrupted)-1]

	if serviceCount := state.PushService(entry); serviceCount > 0 {
		entries := state.GetCurrentServiceStack(entry)[serviceCount-1].Entries
		last := entries[len(entries)-1]
		state.AppendToPEU(last)
	}
}

func handleLOOPSTART(state *State, entry *Entry) {
	if len(state.GetCurrentServiceStack(entry)) > 0 {
		state.PushNewService(entry, entry.Location)
	}
}

func handleLOOPRSTART(state *State, entry *Entry) {
	stack := state.GetCurrentServiceStack(entry)
	if len(stack) == 0 {
		return
	}

	topServiceIdx := len(stack) - 1
	topService := stack[topServiceIdx]
	entries := topService.Entries
	lastEntry := entries[len(entries)-1]

	if lastEntry.EventType == "LOOPSTART" {
		lastEntry.EventType = "LOOPRSTART"
		lastEntry.Cycles = entry.Cycles

		return
	}

	state.PushService(entry)

	service := state.Pop(entry)

	last := len(service.Entries) - 1
	if service.Entries[last].EventType != "LOOPRSTART" {
		panic("Error - LOOPRSTART was not on top of the stack, was " + service.Entries[0].EventType)
	}

	service.Entries[0].EventType = "LOOPSTART"

	// Add service
	state.PushServiceToLEU(entry, service)

	// Create a new entry on the top of the stack
	firstEntry := *service.Entries[0]
	firstEntry.Cycles = entry.Cycles
	firstEntry.EventType = "LOOPRSTART"

	// Do some stack exorcism to fake entering the service
	state.ServiceStack[entry.CPU][state.CurrentLEU[entry.CPU]] =
		append(state.GetCurrentServiceStack(entry), ServiceStackElement{service.Name, []*Entry{}, []int{}})

	state.PushService(&firstEntry)
}

func handleLOOPSTOP(state *State, entry *Entry) {
	if len(state.GetCurrentServiceStack(entry)) < 1 {
		return
	}

	service := state.Pop(entry)

	first := *service.Entries[0]
	first.EventType = "LOOPRSTART"
	first.Cycles = entry.Cycles

	service.Entries = append(service.Entries, &first)

	if service.Entries[0].EventType == "LOOPRSTART" {
		service.Entries[0].EventType = "LOOPSTART"
		state.PushServiceToLEU(entry, service)
	}

	e2 := *entry
	e2.EventType = "LOOPSTOP"
	state.PushService(&e2)
}

func handleSRVQUEUE(state *State, entry *Entry) {
	if entry.GetQueueAction() == ENQUEUE { // Enqueue
		if v, ok := softirqs[entry.Service]; ok {
			entry.Service = v
		}
		state.PushService(entry)
	} else { // Dequeue
		entry.PacketId = "0"
		state.PushNewService(entry, entry.Service)
	}
}

func handlePEUSTART(state *State, entry *Entry) {
	if serviceCount := state.PushService(entry); serviceCount > 0 {
		// Decrypting the appendToPeu() call
		// current leu
		// last service (top)
		// index one
		// last index
		l := state.GetCurrentServiceStack(entry)[serviceCount-1].Entries
		last := l[len(l)-1]
		state.AppendToPEU(last)
	}
}

func handleSync(state *State, entry *Entry) {

	if tempVar, ok := state.TempVars[entry.Service]; ok {
		stack := state.GetCurrentServiceStack(entry)
		if len(stack) > 0 {
			lastService := stack[len(stack)-1]

			if entry.EventType == "SEMUP" || entry.EventType == "COMPL" {
				tempVarEntry := TempVarEntry{lastService.Name, 1}
				tempVar.Entry.TempVars = append(tempVar.Entry.TempVars, tempVarEntry)
			} else { // SEMDOWN || WAITCOMPL
				tempVarEntry := TempVarEntry{lastService.Name, -1}
				tempVar.Entry.TempVars = append(tempVar.Entry.TempVars, tempVarEntry)
			}
		}

		oldType := entry.EventType
		entry.EventType = fmt.Sprintf("%s\t(TEMP)", entry.EventType)

		if serviceCount := state.PushService(entry); serviceCount > 0 {
			// TODO: Make a function for (some of) this
			stack := state.GetCurrentServiceStack(entry)
			last := stack[len(stack)-1]
			entries := last.Entries
			lastEntry := entries[len(entries)-1]

			state.AppendToPEU(lastEntry)
		}

		if oldType == "SEMUP" || oldType == "COMPL" {
			tempVar.Value += 1
		} else {
			tempVar.Value -= 1
		}

		if tempVar.Value == 0 {
			delete(state.TempVars, entry.Service)
		}

	} else {
		if serviceCount := state.PushService(entry); serviceCount > 0 {
			stack := state.GetCurrentServiceStack(entry)
			last := stack[len(stack)-1]
			entries := last.Entries
			lastEntry := entries[len(entries)-1]

			state.AppendToPEU(lastEntry)
		}
	}
}

func handleTEMPSYNCH(state *State, entry *Entry) {
	entry.TempVarId = state.GetTempVarId()
	// entry.TempVarIds = append(entry.TempVarIds, state.GetTempVarId())
	state.PushService(entry)

	state.TempVars[entry.Service] = &TempVar{state.LastAppended, 0, []TempVarEntry{}}
}

func handlePKTQUEUE(state *State, entry *Entry) {
	if entry.EventType == "PKTQUEUEN" && state.LastAppended.EventType == "PKTQUEUE" {
		state.LastAppended.EventType = "PKTQUEUEN"
		entry.EventType = "PKTQUEUE"
		pktextrnr += 1
	} else {
		pktextrnr = 0
	}

	id, _ := strconv.ParseInt(entry.PacketId, 16, 64)
	// size := pktexts[entry.Pid2][pktextrnr]
	packetId := int(id)
	/*
		switch size {
		case 1:
			break
		case 2:
			{
				data := make([]byte, 2)
				binary.BigEndian.PutUint16(data, uint16(id))
				packetId = int(binary.LittleEndian.Uint16(data))
				break
			}
		case 4:
			{
				data := make([]byte, 4)
				binary.BigEndian.PutUint32(data, uint32(id))
				packetId = int(binary.LittleEndian.Uint32(data))
				break
			}
		default:
			{
				panic("Invalid packet size")
				break
			}
		}
	*/

	entry.PacketId = fmt.Sprintf("%d", packetId) // TODO: This should be changed...

	state.PushService(entry)

	for _, stackEntry := range state.GetCurrentServiceStack(entry) {
		stackEntry.PacketIds = append(stackEntry.PacketIds, packetId)
	}
}

func storeCases(state *State) {
	fmt.Println("Storing cases")

	/*
		i := 0
		for leuKey, _ := range state.LEUs {
			for service, _ := range state.LEUs[leuKey] {
				if service != "irq_enter" {
					continue
				}

				caseCount := len(state.LEUs[leuKey][service])
				fmt.Printf("\t%s - %s (%d)\n", leuKey, service, caseCount)

					for _, instance := range state.LEUs[leuKey][service] {
						f, _ := os.Create(fmt.Sprintf("cases/%s-%03d", service, i))
						i += 1
						for _, event := range instance.Entries {
							fmt.Fprintln(f, event.Line)
						}
						f.Close()
					}
			}
		}
	*/
}

func (s *Signature) Key() string {
	return strings.Join(s.Events, "\n")
}

func (s *Signature) Append(str string) {
	s.Events = append(s.Events, str)
}

func (s *Signature) AddDuration(duration int) {
	s.Duration = append(s.Duration, duration)
}

func (p Profile) PutSignature(service string, signature Signature) {
	if _, ok := p[service]; !ok {
		p[service] = make(map[string]*SignatureCollection)
	}

	if existingSignature, ok := p[service][signature.Key()]; ok {
		existingSignature.Durations = append(existingSignature.Durations, signature.Duration)
	} else {
		p[service][signature.Key()] = &SignatureCollection{signature.Events, [][]int{signature.Duration}}
	}
}

func createSignatures(state *State) {
	irqService := "irq_enter"

	fmt.Println("CREATING SIGNATURES")
	profiles := make(Profile)

	for leuKey, _ := range state.LEUs {
		for srvkey, _ := range state.LEUs[leuKey] {

			fmt.Printf("\t%s - %s\n", leuKey, srvkey)

			for _, instance := range state.LEUs[leuKey][srvkey] {
				toDeduct := 0
				starti := 0
				startii := 0
				lastEvent := 0
				signature := Signature{}
				didSleep := false
				previousEntry := &Entry{}

				for _, entry := range instance.Entries {
					locationPadded := ljust(entry.Location, longestLocationString)

					// fmt.Println(*entry)

					// Beginning of IRQ service
					if entry.EventType == "SRVENTRY" && entry.Service == irqService && entry.Service != srvkey {
						starti = entry.Cycles
						toDeduct += entry.Cycles - starti
						continue
					}

					if entry.EventType == "HIRQENTRY" && srvkey != fmt.Sprintf("HIRQ-%d", entry.GetIRQ()) {
						startii = entry.Cycles
						continue
					}

					if entry.EventType == "HIRQEXIT" && srvkey != fmt.Sprintf("HIRQ-%d", entry.GetIRQ()) {
						toDeduct += entry.Cycles - startii
						continue
					}

					if entry.EventType == "LOOPSTART" && entry.Location == srvkey {
						queueOne := GetQueueId(entry.Pid2)

						pktId, err := strconv.ParseInt(entry.PacketId, 16, 64)
						if err != nil {
							panic(err)
						}

						queueTwo := GetQueueId(int(pktId))

						srv, err := strconv.ParseInt(entry.Service, 16, 64)
						if err != nil {
							panic(err)
						}

						signature.Append(fmt.Sprintf("0 LOOPSTART\t\t%d %s %s %d", entry.Pid1, queueOne, queueTwo, int(srv)))
						lastEvent = entry.Cycles
						toDeduct = 0
					}

					// Start of this service
					if (entry.EventType == "SRVENTRY" && entry.Service == srvkey) ||
						(entry.EventType == "SRVQUEUE" && entry.Service == srvkey && entry.GetQueueAction() == DEQUEUE) ||
						(entry.EventType == "HIRQENTRY" && srvkey == fmt.Sprintf("HIRQ-%d", entry.GetIRQ())) {
						signature.Append("0 START")
						lastEvent = entry.Cycles
						toDeduct = 0
					}

					// Start of this service
					if entry.EventType == "LOOPSTART" && entry.Location != srvkey {
						pktId, err := strconv.ParseInt(entry.PacketId, 16, 64)
						if err != nil {
							panic(err)
						}

						srv, err := strconv.ParseInt(entry.Service, 16, 64)
						if err != nil {
							fmt.Printf("%#v\n", *entry)
							panic(err)
						}

						signature.Append(fmt.Sprintf("%s LOOP\t\t%s %d %s %s %d", locationPadded, entry.Location, entry.Pid1, GetQueueId(entry.Pid2), GetQueueId(int(pktId)), srv))
						signature.AddDuration((entry.Cycles - lastEvent) - toDeduct)
						lastEvent = entry.Cycles
						toDeduct = 0
					}

					// End of service
					if (entry.EventType == "SRVEXIT" && entry.Service == srvkey) ||
						(entry.EventType == "HIRQEXIT" && srvkey == fmt.Sprintf("HIRQ-%d", entry.GetIRQ())) {

						// fmt.Printf("cyc: %d - le: %d - delta: %d - deduct: %d\n", entry.Cycles, lastEvent, entry.Cycles-lastEvent, toDeduct)
						signature.AddDuration((entry.Cycles - lastEvent) - toDeduct)
						signature.Append("0 STOP\n")

						toDeduct = 0

						// Store the observed durations for the signature
						profiles.PutSignature(srvkey, signature)
					}

					// End of service
					if entry.EventType == "LOOPRSTART" {
						signature.AddDuration((entry.Cycles - lastEvent) - toDeduct)
						toDeduct = 0
						//inLoop = false
						signature.Append("0 RESTART\n")
						profiles.PutSignature(srvkey, signature)
					}

					if entry.EventType == "TEMPSYNCH" {
						signature.AddDuration((entry.Cycles - lastEvent) - toDeduct)
						toDeduct = 0
						userString := ""
						first := true
						for _, userLEU := range entry.TempVars {
							if first {
								first = false
							} else {
								userString += " "
							}
							userString += fmt.Sprintf("%d %s", userLEU.Value, userLEU.Name)
						}

						signature.Append(fmt.Sprintf("%s TEMPSYNCH\t%s", locationPadded, userString))
						lastEvent = entry.Cycles
					}

					if entry.EventType == "SEMUP" || entry.EventType == "COMPL" || entry.EventType == "SEMDOWN" || entry.EventType == "WAITCOMPL" {
						signature.AddDuration((entry.Cycles - lastEvent) - toDeduct)
						toDeduct = 0
						lastEvent = entry.Cycles

						if idx, ok := addr_map_synch[entry.Service]; ok {
							signature.Append(fmt.Sprintf("%s %s\t\t%s", locationPadded, entry.EventType, synch_ids[idx]))
						} else {
							signature.Append(fmt.Sprintf("%s %s\t\t%s", locationPadded, entry.EventType, entry.Service))
						}

					}

					if strings.Contains(entry.EventType, "(TEMP)") {
						signature.AddDuration((entry.Cycles - lastEvent) - toDeduct)
						toDeduct = 0
						lastEvent = entry.Cycles
						signature.Append(fmt.Sprintf("%s %s", locationPadded, entry.EventType))
					}

					if entry.EventType == "PKTEXTR" || entry.EventType == "PKTEXTRN" {
						signature.AddDuration((entry.Cycles - lastEvent) - toDeduct)
						toDeduct = 0
						lastEvent = entry.Cycles
						signature.Append(fmt.Sprintf("%s %s\t%s", locationPadded, entry.EventType, entry.PacketId))
					}

					if entry.EventType == "PKTQUEUE" ||
						(entry.EventType == "SRVQUEUE" && entry.Service != srvkey) ||
						entry.EventType == "PKTQUEUEN" ||
						entry.EventType == "STATEQUEUE" {
						signature.AddDuration((entry.Cycles - lastEvent) - toDeduct)
						toDeduct = 0

						var operation string
						if entry.Pid1 == 0 {
							if entry.EventType == "PKTQUEUEN" {
								operation = "ENQUEUEN"
							} else {
								operation = "ENQUEUE"
							}
						} else {
							if entry.EventType == "PKTQUEUEN" {
								operation = "DEQUEUEN"
							} else {
								operation = "DEQUEUE"
							}
						}

						queueId := "NOLOC"
						if entry.Pid2 != 0 {
							queueId = GetQueueId(entry.Pid2)
						} else if entry.PacketId != "0" {
							if q, ok := queue_ids[addr_map[entry.PacketId]]; ok {
								queueId = q
							} else {
								queueId = "UNKNOWN"
							}
						} else {
							queueId = "UNKNOWN"
						}

						if operation == "DEQUEUE" && entry.EventType == "SRVQUEUE" {
							entry.Service = "0"
						}

						if entry.EventType == "SRVQUEUE" {
							signature.Append(fmt.Sprintf("%s %s\t\t%s %s %s", locationPadded, operation, entry.EventType, entry.Service, queueId))
						} else if entry.EventType == "STATEQUEUE" {
							pktid, _ := strconv.ParseInt(entry.PacketId, 16, 64)

							queueId := "NOLOC"
							if entry.Pid2 != 0 {
								queueId = GetQueueId(entry.Pid2)
							} else if addr_map_entry, ok := addr_map[entry.PacketId]; ok && entry.PacketId != "0" {
								if q, ok := queue_ids[addr_map_entry]; ok {
									queueId = q
								} else {
									queueId = "UNKNOWN"
								}
							} else {
								queueId = "UNKNOWN"
							}

							signature.Append(fmt.Sprintf("%s %s\t\t%s %d %s %s", locationPadded, operation, entry.EventType, pktid, queueId, GetStateScopeStr(entry.Service)))
						} else {
							signature.Append(fmt.Sprintf("%s %s\t\t%s %s %s", locationPadded, operation, entry.EventType, entry.PacketId, queueId))
						}

						lastEvent = entry.Cycles
					}

					if entry.EventType == "QUEUECOND" {
						signature.AddDuration((entry.Cycles - lastEvent) - toDeduct)
						toDeduct = 0
						lastEvent = entry.Cycles

						signature.Append(fmt.Sprintf("%s QUEUECOND\t%s %s %s", locationPadded, GetQueueId(entry.Pid1), GetQueueId(entry.Pid2), entry.GetQueueCond()))
					}

					if entry.EventType == "THREADCOND" {
						signature.AddDuration((entry.Cycles - lastEvent) - toDeduct)
						toDeduct = 0
						lastEvent = entry.Cycles

						signature.Append(fmt.Sprintf("%s THREADCOND\t%d %s", locationPadded, entry.Pid1, "<Tcond>"))
						panic("Tcond not fully implemented")
					}

					if entry.EventType == "STATECOND" {
						signature.AddDuration((entry.Cycles - lastEvent) - toDeduct)
						toDeduct = 0
						lastEvent = entry.Cycles

						pktid, _ := strconv.ParseInt(entry.PacketId, 16, 64)

						signature.Append(fmt.Sprintf("%s STATECOND\t%s %s %s %d", locationPadded, GetStateIdString(entry.Service), entry.GetStateOperation(), GetStateScope(entry.Pid2), pktid))
					}

					if entry.EventType == "PEUSTART" {
						signature.AddDuration((entry.Cycles - lastEvent) - toDeduct)
						signature.AddDuration(entry.PEUDuration)
						// fmt.Printf("PEU duration: %d - HIRQ-%d\n", entry.PEUDuration, entry.PEUIRQ)
						toDeduct = 0
						signature.Append(fmt.Sprintf("%s PEUSTART HIRQ-%d", entry.Location, entry.PEUIRQ)) // TODO: Figure out this
						lastEvent = entry.Cycles
					}

					// Intra-LEU service call
					if entry.EventType == "SRVENTRY" && entry.Service != srvkey && entry.Service != irqService {
						signature.Append(fmt.Sprintf("%s CALL\t\t%s", locationPadded, entry.Service))
						signature.AddDuration((entry.Cycles - lastEvent) - toDeduct)
						toDeduct = 0
					}

					// Done with intra-LEU call
					if entry.EventType == "SRVEXIT" && entry.Service != srvkey && entry.Service != irqService {
						lastEvent = entry.Cycles
						toDeduct = 0
					}

					if entry.EventType == "LOOPSTOP" {
						lastEvent = entry.Cycles
						toDeduct = 0
					}

					if entry.EventType == "TTWAKEUP" && previousEntry.EventType != "SEMUP" && previousEntry.EventType != "COMPL" {
						signature.AddDuration((entry.Cycles - lastEvent) - toDeduct)
						toDeduct = 0
						lastEvent = entry.Cycles

						var leuName string
						var ok bool
						if leuName, ok = leuNames[entry.Pid1]; !ok {
							leuName = fmt.Sprintf("UNKNOWN-LEU(pid=%d)", entry.Pid1)
						}

						signature.Append(fmt.Sprintf("%s TTWAKEUP\t%s %s", locationPadded, leuName, entry.PacketId))
					}

					if entry.EventType == "SLEEP" {
						didSleep = true
						continue // Avoid setting previous entry further down
					}

					if entry.EventType == "CTXSW" {
						if entry.Pid1 == leuKey.Pid {
							// Switched out
							if didSleep && !strings.Contains(previousEntry.EventType, "SEMDOWN") && !strings.Contains(previousEntry.EventType, "WAITCOMPL") {
								signature.Append(fmt.Sprintf("%s SLEEP", locationPadded))
								signature.AddDuration((entry.Cycles - lastEvent) - toDeduct)
								toDeduct = 0
							} else {
								starti = entry.Cycles
							}
						} else if entry.Pid2 == leuKey.Pid {
							// Switched in
							if didSleep {
								didSleep = false
								lastEvent = entry.Cycles
							} else {
								toDeduct += entry.Cycles - starti
								continue
							}
						}
					}

					previousEntry = entry
				}
			}
		}
	}

	fmt.Println("WRITING SIGNATURES")
	storeSignatures(profiles)
}

func storeSignatures(profiles Profile) {
	// For each service
	for serviceName, _ := range profiles {
		profile := profiles[serviceName]
		signatureCount := 0
		fractions := make(FractionSlice, 0)

		// Find the total number of signatures for this service
		for _, signature := range profile {
			signatureCount += len(signature.Durations)
		}

		// Create a fraction
		for _, signature := range profile {
			thisCount := len(signature.Durations)

			fraction := Fraction{
				float64(thisCount) / float64(signatureCount) * 100.0,
				thisCount,
				signatureCount,
				*signature,
			}

			fractions = append(fractions, fraction)
		}

		// Sort the fractions to find the most common signature(s)
		sort.Sort(fractions)

		// Write them
		for fractionNr, fraction := range fractions {
			filename := fmt.Sprintf("./signatures.mine/%s0SIGNR%d", serviceName, fractionNr)
			f, err := os.OpenFile(filename, os.O_CREATE|os.O_TRUNC|os.O_WRONLY, 0666)
			if err != nil {
				fmt.Println(err)
				return
			}

			write := func(format string, args ...interface{}) {
				f.WriteString(fmt.Sprintf(format, args...))
			}

			if f == os.Stdout {
				fmt.Fprintln(f, filename)
			}

			averages := make(map[int]float64)
			count := len(fraction.Durations)
			for _, durations := range fraction.Durations {
				for psindex, duration := range durations {
					if avg, ok := averages[psindex]; ok {
						averages[psindex] = avg + float64(duration)/float64(count)
					} else {
						averages[psindex] = float64(duration) / float64(count)
					}
				}
			}

			sds := make(map[int]float64)
			for _, durations := range fraction.Durations {
				for psindex, duration := range durations {
					if sd, ok := sds[psindex]; ok {
						sds[psindex] = sd + math.Abs(float64(duration)-averages[psindex])
					} else {
						sds[psindex] = math.Abs(float64(duration) - averages[psindex])
					}
				}
			}

			for i, _ := range sds {
				sds[i] /= float64(count)
			}

			write("SIGSTART\nNAME %s\nPEU cpu\nRESOURCES cycles normal\n", serviceName)

			write("FRACTION %d%%  %d %d\n\n", int(fraction.Percent), fraction.Count, fraction.Total)

			if f == os.Stdout {
				fmt.Fprintln(f)
			}

			delayInd := 0
			events := fraction.SignatureCollection.Events
			for i, signaturePart := range events {
				write("%s", signaturePart)

				if strings.Contains(signaturePart, "PEUSTART") {
					write(" %d %d", int(averages[delayInd]), int(sds[delayInd]))
					delayInd += 1
				}

				write("\n")

				if i < len(events)-1 {
					write("x                                PROCESS\t\t%d %d\n", int(averages[delayInd]), int(sds[delayInd]))
				}

				delayInd += 1
			}

			write("SIGEND\n\n")
			f.Close()
		}
	}
}

func toEntry(a []string) (entry *Entry) {
	entry = new(Entry)

	atoi := func(str string) (i int) {
		i, err := strconv.Atoi(str)
		if err != nil {
			panic(err)
		}
		return
	}

	entry.Cycles = atoi(a[0])
	entry.EventType = a[1]
	entry.CurrentPid = atoi(a[2])
	entry.Pid1 = atoi(a[3])
	entry.Pid2 = atoi(a[4])
	entry.PacketId = a[5]
	entry.Service = a[6]
	entry.Location = a[7]
	entry.LineNr = atoi(a[8])

	return
}

func loadCase(name string) (l LEUentry) {
	entryExp := regexp.MustCompile(
		// [715056, 'LOOPSTART', 10, 0, 24, '20', 'a', '__do_softirq+0x7c', 889]
		"\\[(\\d+), '(\\w+)', (\\d+), (\\d+), (\\d+), '([^']*)', '([^']*)', '([^']*)', (\\d+)\\]")

	f, err := os.Open(name)
	if err != nil {
		panic(err)
	}

	scanner := bufio.NewScanner(f)
	scanner.Split(bufio.ScanLines)

	for scanner.Scan() {
		line := scanner.Text()

		m := entryExp.FindAllStringSubmatch(line, -1)
		for _, ma := range m {
			entry := toEntry(ma[1:])
			l.Entries = append(l.Entries, entry)
		}
	}

	return
}

func createSignature( /*cases LEUentry*/ ) {
	srvkey := "__do_softirq+0x7c"
	cases := loadCase(srvkey)

	irqService := "irq_enter"

	toDeduct := 0
	lastEvent := 0
	// starti := 0
	durations := []int{}
	allDurations := [][]int{}

	pushDuration := func(entry *Entry) {
		durations = append(durations, (entry.Cycles-lastEvent)-toDeduct)
	}

	printDurations := func() {
		// fmt.Print(durations[0])
		// for _, dur := range durations[1:] {
		// fmt.Printf(" %d", dur)
		//}
		// fmt.Println()

		allDurations = append(allDurations, durations)

		// if len(durations) != 2 { panic("len") }
	}

	for _, entry := range cases.Entries {
		// fmt.Printf("%v\n", entry)
		fmt.Println(entry.EventType)

		handled := false

		// Beginning of OS IRQ service
		if entry.EventType == "SRVENTRY" && entry.Service == irqService && entry.Service != srvkey {
			// starti = entry.Cycles
			continue
		}

		// HIRQENTRY
		// HIRQEXIT

		if entry.EventType == "LOOPSTART" && entry.Location == srvkey {
			handled = true
			lastEvent = entry.Cycles
			toDeduct = 0
		}

		// Start of this service
		if (entry.EventType == "SRVENTRY" && entry.Service == srvkey) ||
			(entry.EventType == "SRVQUEUE" && entry.Service == srvkey && entry.GetQueueAction() == DEQUEUE) {
			handled = true
			lastEvent = entry.Cycles
			toDeduct = 0
		}

		// Start of this service
		if entry.EventType == "LOOPSTART" && entry.Location != srvkey {
			handled = true
			pushDuration(entry)
			lastEvent = entry.Cycles
			toDeduct = 0
		}

		// Finished with this service
		if entry.EventType == "SRVEXIT" && entry.Service == srvkey {
			handled = true
			pushDuration(entry)
			toDeduct = 0

			printDurations()
			durations = []int{}
		}

		// Finished with this service
		if entry.EventType == "LOOPRSTART" {
			handled = true
			pushDuration(entry)
			toDeduct = 0

			printDurations()
			durations = []int{}
		}

		// TEMPSYNC
		// SEMUP COMPL SEMDOWN WAITCOMPL
		// (TEMP)
		// PKTEXTR(N)

		if entry.EventType == "SRVQUEUE" && entry.Service != srvkey {
			handled = true
			pushDuration(entry)
			toDeduct = 0
			lastEvent = entry.Cycles
		}

		if entry.EventType == "QUEUECOND" {
			handled = true
			pushDuration(entry)
			toDeduct = 0
			lastEvent = entry.Cycles
		}

		// THREADCOND
		// STATECOND
		// PEUSTART

		// Intra-LEU service call
		if entry.EventType == "SRVENTRY" && entry.Service != srvkey && entry.Service != irqService {
			handled = true
			pushDuration(entry)
		}

		// Finished with intra-LEU service
		if entry.EventType == "SRVEXIT" && entry.Service != srvkey && entry.Service != irqService {
			handled = true
			toDeduct = 0
			lastEvent = entry.Cycles
		}

		if entry.EventType == "LOOPSTOP" {
			handled = true
			lastEvent = entry.Cycles
			toDeduct = 0
		}

		if entry.EventType == "SRVEXIT" && entry.Service != srvkey && entry.Service == irqService {
			handled = true
		}

		if !handled {
			// fmt.Printf("%+v\n", entry)
			// fmt.Println("Unhandled")
			panic("Unhandled")
		}
	}

	averages := make(map[int]float64)
	count := len(allDurations)
	for _, durations := range allDurations {
		for psindex, duration := range durations {
			if avg, ok := averages[psindex]; ok {
				averages[psindex] = avg + float64(duration)
			} else {
				averages[psindex] = float64(duration)
			}
		}
	}

	for i, _ := range averages {
		averages[i] /= float64(count)
	}
}
