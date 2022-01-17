import os
import sys
print("create {0} client connections".format(sys.argv[1]))
for i in range(int(sys.argv[1])):
    os.system("./timeout_client.out localhost 8080&")
