from graph_tutorial_6 import JustMoveOn, NothingToSeeHere
from utility import uint

#------------------------------------------------------------------------------
#
# 8 - EXPANDING THE SCOPE
#  Showing only one level of dependencies in the graph works well if you want
#  to get a quick overview on the parts of your codebase that are related to a
#  certain symbol. So using the graph makes sure that you don't overlook any
#  relation while planning to modify the source code. But sometimes that is not
#  enough.
#  Sometimes you are interested in a whole call graph for a certain function or
#  you want to see the complete inheritance chain of a specific class and you
#  want to expand the scope shown by the graph.
#  Do you see the small icon on the upper left of the graph? Use it to show the
#  depth graph controls and click the "show base hierarchy" button.
#
#------------------------------------------------------------------------------


class ClickMeLater(JustMoveOn, NothingToSeeHere):

	def __init__(self):
		self.value: uint = 0

	def produce(self):
		self.value += 1

	def consume(self):
		self.value -= 1

	def isPositive(self):
		return self.value > 0
