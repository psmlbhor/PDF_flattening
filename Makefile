FLAGS+=$(shell pkg-config --cflags --libs libqpdf)
CXXFLAGS+=-Wall

all: Flatten

Flatten: Flatten.cc
	$(CXX) $^ -o $@ $(CXXFLAGS) $(FLAGS)

clean:
	-rm Flatten
