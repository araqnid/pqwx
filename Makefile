CXXFLAGS := -I$(shell pg_config --includedir) $(shell wx-config --cxxflags)
LDFLAGS := -L$(shell pg_config --libdir) -lpq $(shell wx-config --libs)
OBJS := server_connection.o pqwx.o pqwx_frame.o object_browser.o database_connection.o database_work.o

pqwx: $(OBJS)
	g++ $(LDFLAGS) -o $@ $^

%.o: %.cpp
	g++ $(CXXFLAGS) -c -o $@ $^

clean:
	rm -f $(OBJS) pqwx
