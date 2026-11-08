from utility import uint

#------------------------------------------------------------------------------
#
# 1 - THE GRAPH TUTORIAL
#  In Sourcetrail's default window layout the graph is shown to the left, next to the
#  code. It displays the structure of your project where all named symbols are
#  nodes and all relations are edges. Like the code view it shows only the
#  currently active symbol and all the incoming and outgoing dependencies.
#  If you want to see which node and edge types exist, then click the '?'-icon in the
#  lower right corner of the graph view to show the graph legend.
#
# 2 - HOW DO I NAVIGATE THE GRAPH
#  You can activate a new symbol by left-clicking on the node you are
#  interested in. For example: to find out where the function
#  "where_am_i_called()" is called get over to the graph and click that
#  function's node.
#
#------------------------------------------------------------------------------


class GraphTutorial:

	def __init__(self):
		self.step_count: uint = 0

	@staticmethod
	def where_am_i_called():
		pass
