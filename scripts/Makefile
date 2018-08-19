
SOURCES = $(wildcard exp*.pcap)

TARGETS=$(SOURCES:.pcap=.stats)

all: $(TARGETS)

segments.bak: $(wildcard capture*.pcap)
	tshark -t ad -r $< -Y "udp && not icmp && (udp.port == 4123 && wlan.sa == e0:9d:31:57:14:10)" | cut --delimiter=" " -f 2-3,13,15 > $@
#	./split $<

%.tsharkstats: %.pcap
	tshark -r $< -qz io,stat,1,"COUNT(frame.len)frame.len && pktgen && wlan.sa == a0:0b:ba:cc:d4:37" > $@


%.stats: segments
%.stats: %.tsharkstats
	../scripts/stats_parse $< > $@

titles: output.log
	grep "HEADER" $^ | cut --delimiter=" " -f 10,13,16,18 | sed "s/\\_/\\\\_/g" > $@

DELAYSRC = $(wildcard traces/*.trace)
DELAY_TARGETS = $(DELAYSRC:.trace=.delay)

delays: $(DELAY_TARGETS)

CDF_TARGETS = $(patsubst traces/%.delay,traces/%cdf.pdf,$(DELAY_TARGETS))
cdf: $(CDF_TARGETS)

DELAYPLOT_TARGETS = $(patsubst traces/%.delay,traces/%delay.pdf,$(DELAY_TARGETS))
delayplot: $(DELAYPLOT_TARGETS)

traces/%delay.pdf: traces/%.delay
	$(eval title := $(shell ../scripts/gentitle1.sh $^))
	../scripts/plotdelay.sh $^ $@ "$(title)"

traces/%cdf.pdf: traces/%.delay
	$(eval title := $(shell ../scripts/gentitle1.sh $^))
	../scripts/plotcdf.sh $^ $@ "$(title)"

traces/%.delay: traces/%.trace
	../scripts/intraosdelay $^ | pcregrep -o1 "DELTA\(\d+\): (\d+)" > $@

traces/%.migr: traces/%.trace
	../scripts/migrationdelay -migrate=93 $^ | pcregrep -o1 -o2 --om-separator=" " "^DELTAMIGR\(\d+\): (\d+) (\d+)$$" > $@

traces/%.migr.pdf: traces/%.migr
	echo -e "set terminal pdf; set output \"$@\"; plot \"$^\" using (\$$2):(\$$1) with points;" | gnuplot

MIGRPLOT_TARGETS = $(patsubst traces/%.trace,traces/%.migr.pdf,$(DELAYSRC))
migrplot: $(MIGRPLOT_TARGETS)


%.pdf: titles
%.pdf: %.stats
	$(eval title := $(shell ../scripts/gentitle0.sh $^))
	../scripts/genplot.sh $< $@ "$(title)"

PDFSOURCES=$(wildcard exp*.pcap)
PDFTARGETS=$(PDFSOURCES:.pcap=.pdf)

pdf: $(PDFTARGETS)

all.pdf: $(wildcard exp*.pdf)
	pdfunite $(shell ls -v *.pdf) $@

clean:
	rm -f exp*.pdf all.pdf
	rm -f exp*.pcap
	rm -f exp*.stats

mrproper:
	rm -f segments
	rm -f titles
