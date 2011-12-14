CXXFLAGS := -g -I$(shell pg_config --includedir) $(shell wx-config --debug=yes --cxxflags)
LDFLAGS := -L$(shell pg_config --libdir) -lpq $(shell wx-config --debug=yes --libs)
OBJS := server_connection.o pqwx.o pqwx_frame.o object_browser.o database_connection.o

pqwx: $(OBJS)
	g++ $(LDFLAGS) -o $@ $^

-include $(OBJS:.o=.d)

%.o: %.cpp
	g++ $(CXXFLAGS) -c -o $@ $*.cpp
	@g++ $(CXXFLAGS) -MM -o $*.d $*.cpp

clean:
	rm -f *.o *.d pqwx
