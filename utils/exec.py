#!/usr/bin/env python3

import argparse
import subprocess

#
# Main
#

class Main:

    def readConfig(self, configPath):
        hosts = list()
        with open(configPath) as f:
            skip = False
            for l in f:
                line = l.strip()
                if line.startswith('@bash_begin@'):
                    skip = True
                elif line.startswith('@bash_end@'):
                    skip = False
                elif line.startswith('#') or len(line) == 0:
                    continue
                else:
                    if skip : continue
                    values = line.strip().split(',')
                    if not len(values) == 5: 
                        raise RuntimeError("ERROR: failed to parse line ({0})".format(line))
                    hosts.append(values[1].strip())
        print("Configuration read successfully from {0}".format(configPath))
        print("Hosts ({0}) are ({1})".format(len(hosts), hosts))
        return hosts

    def execAll(self, hosts, cmd):
        for host in hosts:
            print("Executing on {0}".format(host))
            subprocess.run("ssh {0} {1};".format(host, cmd), stderr=subprocess.STDOUT, shell = True)

    def make_args_parser(self):
        parser = argparse.ArgumentParser(prog='exec.py')
        parser.add_argument('--cmd', default=None, required=True, help='Command to be executed')
        parser.add_argument('--cfg', default=None, help='Path to DDS SSH configuration file')
        parser.add_argument('--hosts', default=None, help='Comma seperated list of hosts')
        return parser

    # Main function
    def main(self, argv):
        parcer = self.make_args_parser()
        args = parcer.parse_args()

        if args.cfg != None:
            self.execAll(self.readConfig(args.cfg), args.cmd)
        if args.hosts != None:
            self.execAll(args.hosts.strip().split(','), args.cmd)

# Main
if __name__ == "__main__":
    import sys
    Main().main(sys.argv)
