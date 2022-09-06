#!/usr/bin/env python

import taskmap
import asyncio
import logging

class Scenario:

	def __init__(self, actions_dict, dependencies_dict, log=False):

		# dictionnary of actions (action_name, function_name)
		self.actions_dict = actions_dict

		# dictionnary of dependencies
		self.dependencies_dict = dependencies_dict

		self.scenario_graph = taskmap.create_graph(actions_dict, dependencies_dict)

		if not log: logging.disable(logging.CRITICAL)

	'''
		@brief performs one forward pass of the graph
		meaning that the scenario is finished if no error occured

		@return{taskmap graph(dict)} the result of one forward pass
	'''
	def exec_once(self):
		taskmap.run_async(self.scenario_graph, sleep=.001)

	'''
		@brief performs one forward pass of the graph
		meaning that the scenario is finished if no error occured

		@return{taskmap graph(dict)} the result of one forward pass
	'''
	def exec_once_parallel(self):
		taskmap.run_parallel_async(self.scenario_graph, nprocs=2, sleep=.001)

	'''
		@brief performs one forward pass of the graph
		meaning that the scenario is finished if no error occured
		/!\ + raise erros

		@return{taskmap graph(dict)} the result of one forward pass
	'''
	def exec_once_raise_errors(self):
		taskmap.run_async(self.scenario_graph, sleep=.001, raise_errors=True)

	'''
		@brief performs one forward pass of the graph
		meaning that the scenario is finished if no error occured
        parallel + sync

		@return{taskmap graph(dict)} the result of one forward pass
	'''
	def exec_once_sync_parallel(self, ncores):
		taskmap.run_parallel(self.scenario_graph, nprocs=ncores)

	def update_scenario_graph_after_exception(self):
		self.scenario_graph = taskmap.reset_failed_tasks(self.scenario_graph)
		self.scenario_graph = taskmap.create_graph(
			self.scenario_graph.funcs,
			self.scenario_graph.dependencies,
			io_bound=None,
			done=self.scenario_graph.done,
			results=self.scenario_graph.results,
			name='taskmap',
			logging_config=None
			)

	def exec_till_complete_or_n(self, nb_turn):
		i = 0
		is_not_completed = True
		while is_not_completed and i<nb_turn:
			try:
				self.exec_once_raise_errors()
				is_not_completed = False
			except Exception as e:
				i += 1
				if any(isinstance(t, Exception) for a,t in self.scenario_graph.results.items()):
					self.update_scenario_graph_after_exception()
				else:
					print("An race condition happen, could'not be catched")
					raise e
