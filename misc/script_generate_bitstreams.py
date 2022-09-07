#!/usr/bin/env python

from scenario import Scenario

import asyncio
import os
import math
#sys.path.append('~/Documents/PhD/high_end/misc/')
from configs import configs

cmd_implem_snap = "make image -C /users/lledoux/Documents/SA/scratchpad_ocaccel/tmp/SA_200_{}_{}_{}_{}_m{}_{}_HSSD"

actions_push = {}
dependencies_push = {}

for c in configs:
    N = c[2]
    M = c[3]
    fct_implem_snap = "fct_implem_ocaccel_"+ str(N) + "_" + str(M) + "_" + str(c[1]).replace(":","_")+"_"+str(c[7])+"_m"+str(-c[8])+"_" + str(c[9])
    exec(
        'def {}():'.format(fct_implem_snap) +
		'\n\tos.system("{}")'.format(cmd_implem_snap.format(N,M,c[1].replace(":","_"),c[7],-c[8],c[9]))
    )
    actions_push[fct_implem_snap] = eval(fct_implem_snap)
    dependencies_push[fct_implem_snap]=[]

def place_and_route_and_report():

    # create the actions dictionary
    actions = actions_push
    #actions["fct_cpreport_200_5"]()

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
    place_and_route_and_report()

if __name__ == '__main__':
    main()
