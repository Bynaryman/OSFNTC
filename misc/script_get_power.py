#!/usr/bin/env python

from scenario import Scenario

import asyncio
import os
import math
import sys
sys.path.append('.')
from configs import configs

cmd_report_power_utilization = "vivado -mode batch -source {}"

"""
    dynamically does that:
    Create an adequate tcl script that will open routed opt dcp and report power and utilizations
	Lunch vivado to exectute this tcl in mode batch
    Add the dependency between the copy i and the pnr i
"""
actions_push = {}
dependencies_push = {}
for c in configs:
    name = "/users/lledoux/Documents/SA/scratchpad_ocaccel/opt_routed_dcps/tclscript_SA_200_{}_{}_{}_{}_m{}_{}_HSSD.tcl".format(c[2],c[3],c[1].replace(":","_"),c[7],abs(c[8]),c[9])
    tclscriptfile = open(name, "w")
    tclscriptfile.write("open_checkpoint /users/lledoux/Documents/SA/scratchpad_ocaccel/opt_routed_dcps/SA_200_{}_{}_{}_{}_m{}_{}_HSSD.dcp\n".format(c[2],c[3],c[1].replace(":","_"),c[7],abs(c[8]),c[9]))
    tclscriptfile.write("report_utilization -hierarchical -file /users/lledoux/Documents/SA/opt_routed_dcps/SA_200_{}_{}_{}_{}_m{}_{}_HSSD_utilization_opt_routed.rpt\n".format(c[2],c[3],c[1].replace(":","_"),c[7],abs(c[8]),c[9]))
    tclscriptfile.write("report_power -file /users/lledoux/Documents/SA/opt_routed_dcps/SA_200_{}_{}_{}_{}_m{}_{}_HSSD_power_opt_routed.rpt\n".format(c[2],c[3],c[1].replace(":","_"),c[7],abs(c[8]),c[9]))
    tclscriptfile.close()

for c in configs:
    fct_report_power_utilization_name = "fct_power_report_utilization_SA_200_{}_{}_{}_{}_m{}_{}_HSSD".format(c[2],c[3],c[1].replace(":","_"),c[7],abs(c[8]),c[9])
    name = "/users/lledoux/Documents/SA/scratchpad_ocaccel/opt_routed_dcps/tclscript_SA_200_{}_{}_{}_{}_m{}_{}_HSSD.tcl".format(c[2],c[3],c[1].replace(":","_"),c[7],abs(c[8]),c[9])
    exec(
        'def {}():'.format(fct_report_power_utilization_name) +
        '\n\tos.system("{}")'.format(cmd_report_power_utilization.format(name))
    )
    actions_push[fct_report_power_utilization_name] = eval(fct_report_power_utilization_name)
    dependencies_push[fct_report_power_utilization_name]=[]

def report_power_utilization():

    # create the actions dictionary
    actions = actions_push

    # create the action dependencies dictionnary
    dependencies = dependencies_push
    print(actions)
    print("========================================")
    print(dependencies)
    # then create the scenario
    pNrNreport = Scenario(actions, dependencies, log=True)
    #pNrNreport = Scenario(actions, dependencies, log=False)

    # launch the scenario until it succeed
    pNrNreport.exec_once_sync_parallel(12)


def main():

    # create and play a run
    report_power_utilization()

if __name__ == '__main__':
    main()

