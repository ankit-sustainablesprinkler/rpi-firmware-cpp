#include <iostream>
#include <sys/file.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <vector>

using namespace std;

const char *s_list[] = {"A","A#","B","C","C#","D","D#","E","F","F#","G","G#"};

int main()
{
	int pid_file = open("/tmp/sched_test.pid", O_CREAT | O_RDWR, 666);
	int rc = flock(pid_file, LOCK_EX | LOCK_NB);
	if(rc && EWOULDBLOCK == errno) {
		// another instance is running
	}
	else {
		// this is the first instance
		int i = 0;
		while(true){
			cout << s_list[i++] << endl;
			
			sleep(1);
		}
	}
	return 0;
}