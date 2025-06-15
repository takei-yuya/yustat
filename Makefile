CC=$(CXX)
SRCS=$(wildcard src/*.cpp)
OBJS=$(SRCS:src/%.cpp=build/%.o)
BINS=$(SRCS:src/%.cpp=build/%)

SERVICE_IN_FILES=$(wildcard service/*.service.in)
SERVICE_FILES=$(SERVICE_IN_FILES:service/%.service.in=build/%.service)

systemd_server_option=--user
prefix=$(HOME)/.local
bindir=$(prefix)/bin
servicedir=$(HOME)/.config/systemd/user

.PHONY: all
all: $(BINS) $(SERVICE_FILES)

.PHONY: clean
clean:
	$(RM) -r build/

.PHONY: install
install: all
	install -d $(bindir)
	install -Dm755 $(BINS) $(bindir)
	install -d $(servicedir)
	install -Dm644 $(SERVICE_FILES) $(servicedir)
	systemctl $(systemd_server_option) daemon-reload
	systemctl $(systemd_server_option) enable --now $(basename $(notdir $(SERVICE_FILES)))
	systemctl $(systemd_server_option) restart $(basename $(notdir $(SERVICE_FILES)))

.PHONY: uninstall
uninstall:
	systemctl $(systemd_server_option) daemon-reload
	systemctl $(systemd_server_option) disable --now $(basename $(notdir $(SERVICE_FILES)))
	$(foreach bin,$(BINS),$(RM) $(bindir)/$(notdir $(bin));)
	$(foreach service,$(SERVICE_FILES),$(RM) $(servicedir)/$(notdir $(service));)

build/%: build/%.o
	$(LINK.o) $^ $(LOADLIBES) $(LDLIBS) -o $@

build/%.o: src/%.cpp
	@mkdir -p build
	$(COMPILE.cpp) $(OUTPUT_OPTION) $<

build/%.service: service/%.service.in
	@mkdir -p build
	sed -e 's|@BINDIR@|$(bindir)|g' $< > $@
