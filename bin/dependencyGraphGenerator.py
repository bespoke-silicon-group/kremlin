#!/usr/bin/env python
from sys import exit

class Function:
	def __init__(self,name,raw_lines):
		self.name = name
		self.basic_blocks = []
		self.subregions = [] # list of top level regions
		self.top_level_blocks = [] # list of basic blocks that aren't in subregion
		self.name_to_node = dict()
		self.name_to_node["MEM"] = Node("MEM","MEM")
		self.process_raw_lines(raw_lines)
		if len(self.basic_blocks) != 0:
			self.form_subregions()

	def process_raw_lines(self,raw_lines):
		curr_bb_name = ""
		raw_bb_lines = []

		for line in raw_lines:
			if len(line) == 0: continue
			elif "BB_BEGIN" in line:
				assert len(curr_bb_name) == 0, "starting new bb before ending previous one (%s)" % curr_bb_name
				curr_bb_name = line.split(': ')[1]
			elif "BB_END" in line:
				end_bb_name = line.split(': ')[1]
				assert curr_bb_name == end_bb_name, "ending bb (%s) that wasn't active bb (%s)" % (end_bb_name,curr_bb_name)
				new_bb = BasicBlock(curr_bb_name,self.name_to_node,raw_bb_lines)
				self.basic_blocks.append(new_bb)
				curr_bb_name = ""
				raw_bb_lines = []
			else:
				raw_bb_lines.append(line)

	def form_subregions(self):
		region_stack = []

		entry_bb = self.basic_blocks[0]
		self.process_bb(entry_bb,region_stack)

	def process_bb(self,basic_block,region_stack):
		basic_block.processed = True

		region_stack_copy = [reg for reg in region_stack]

		for region_id,region_type,event in basic_block.region_events:
			if event == "enter":
				reg = Region(region_id,region_type)

				if len(region_stack_copy) != 0:
					region_stack_copy[-1].subregions.append(reg)
				else:
					self.subregions.append(reg)

				region_stack_copy.append(reg)

			elif event == "exit":
				assert region_stack_copy[-1].id == region_id, "%s exited but %s is top of stack" % (region_id, region_stack_copy[-1].id)
				region_stack_copy.pop()
			else:
				assert False, "unknown event type: %s" % event

		if len(region_stack_copy) != 0:
			region_stack_copy[-1].basic_blocks.append(basic_block)
		else:
			self.top_level_blocks.append(basic_block)

		basic_blocks_to_process = [bb for bb in basic_block.next_basic_blocks if not bb.processed]
		for bb in basic_blocks_to_process:
			self.process_bb(bb,region_stack_copy)

	def write_dot_graph(self):
		print "Not implemented yet"

class Region:
	def __init__(self,id,type):
		self.id = id
		self.type = type
		self.subregions = []
		self.basic_blocks = []

class BasicBlock:
	def __init__(self,name,name_to_node,raw_lines):
		#print "processing BB: %s" % name
		self.name = name
		self.nodes = []
		self.edges = []
		self.parent_region = None
		self.next_basic_blocks = []
		self.region_events = []
		self.callsite_name = "" # used for handling KPrepCall neatly
		self.arg_links = [] # used for adding edges on KLinkArg
		self.return_link = "" # used for handling KLinkReturn
		self.name_to_node = name_to_node
		self.processed = False
		self.process_raw_lines(raw_lines)

	def process_raw_lines(self,raw_lines):
		for line in raw_lines:
			line = line.strip()
			if "CALL" in  line: self.process_call(line)
			elif "RETURN" in line: continue
			elif "TERMINATOR" in line: self.process_terminator(line)
			else: self.process_lib_call(line)

	def process_call(self,line):
		# strip off the CALL: part
		line = line[5:].strip()

		if "=" in line:
			line_splits = line.split('= ')
			line = line_splits[1]

		called_func_name = line[1][0:line[1].find('(')+1]
		cs_name = ""
		if self.callsite_name != "":
			cs_name = self.callsite_name

		call_node = CallNode(called_func_name,cs_name)
		self.nodes.append(call_node)
		self.name_to_node[call_node.name] = call_node
		# add edges from linked args to call
		for idx in range(len(self.arg_links)):
			self.edges.append((self.name_to_node[self.arg_links[idx]],call_node))
		# add edge from call to return link if one exists
		if self.return_link != "":
			self.edges.append((call_node,self.name_to_node[self.return_link]))

		# clear out (now obsolete) callsite name, arg links, and return link
		self.callsite_name = ""
		self.arg_links = []
		self.return_link = ""

	def process_terminator(self,term_string):
		# strip off TERMINATOR: part
		term_string = term_string[11:].strip()

		self.basic_blocks = term_string.split(', ')

	def process_lib_call(self,line):
		""" Returns pair of func name and list of arg strings"""
		def parse_lib_call_string(call_string):
			# first in list should be name, second should be arg list string
			#call_string_splits = call_string.split('()')
			#print call_string_splits

			open_paren_idx = call_string.find('(')
			called_func_name = call_string[0:open_paren_idx]
			args = call_string[open_paren_idx+1:-1]

			# split arg list string by comma
			arg_splits = map(lambda str: str.strip(),args.split(','))
			return (called_func_name,arg_splits)

		func_name, args = parse_lib_call_string(line)

		if "_KTimestamp" == func_name:
			dest_name = "Reg" + args[0]
			dest_node = self.name_to_node[dest_name]
			dest_node.type = "TS";
			num_deps = int(args[1])
			for idx in range(num_deps):
				dependency_name = "Reg" + args[2+2*idx]
				self.edges.append((self.name_to_node[dependency_name],dest_node))
		elif "_KTimestamp0" == func_name:
			dest_name = "Reg" + args[0]
			dest_node = self.name_to_node[dest_name]
			dest_node.type = "TS0";
		elif "_KTimestamp1" == func_name:
			dest_name = "Reg" + args[0]
			dest_node = self.name_to_node[dest_name]
			dest_node.type = "TS1";
			dependency_name = "Reg" + args[1]
			self.edges.append((self.name_to_node[dependency_name],dest_node))
		elif "_KTimestamp2" == func_name:
			dest_name = "Reg" + args[0]
			dest_node = self.name_to_node[dest_name]
			dest_node.type = "TS2";

			dep1_name = "Reg" + args[1]
			self.edges.append((self.name_to_node[dep1_name],dest_node))
			dep2_name = "Reg" + args[3]
			self.edges.append((self.name_to_node[dep2_name],dest_node))
		elif "_KLoad" == func_name:
			dest_name = "Reg" + args[1]
			dest_node = self.name_to_node[dest_name]
			dest_node.type = "LD";

			self.edges.append((self.name_to_node["MEM"],dest_node))
			num_deps = int(args[3])
			for idx in range(num_deps):
				dependency_name = "Reg" + args[4+idx]
				self.edges.append((self.name_to_node[dependency_name],dest_node))

		elif "_KLoad0" == func_name:
			dest_name = "Reg" + args[1]
			dest_node = self.name_to_node[dest_name]
			dest_node.type = "LD0";

			self.edges.append((self.name_to_node["MEM"],dest_node))
		elif "_KLoad1" == func_name:
			dest_name = "Reg" + args[1]
			dest_node = self.name_to_node[dest_name]
			dest_node.type = "LD1";

			self.edges.append((self.name_to_node["MEM"],dest_node))
			dependency_name = "Reg" + args[2]
			self.edges.append((self.name_to_node[dependency_name],dest_node))
		elif "_KLoad2" == func_name:
			dest_name = "Reg" + args[1]
			dest_node = self.name_to_node[dest_name]
			dest_node.type = "LD2";

			self.edges.append((self.name_to_node["MEM"],dest_node))
			dep1_name = "Reg" + args[2]
			self.edges.append((self.name_to_node[dep1_name],dest_node))
			dep2_name = "Reg" + args[3]
			self.edges.append((self.name_to_node[dep2_name],dest_node))
		# TODO: With current method, store won't show what BB the store
		# happened in. Need to fix this. Might do this by creating an edge class
		# that allows us to specify a label (which can indicate BB where store
		# happened.
		elif "_KStore" == func_name:
			mem_node = self.name_to_node["MEM"]
			dependency_name = "Reg" + args[0]
			self.edges.append((self.name_to_node[dependency_name],mem_node))
		elif "_KPrepCall" == func_name:
			if self.callsite_name != "": sys.exit("last callsite_name not cleared")
			self.callsite_name = args[0]
		elif "_KLinkArg" == func_name:
			self.arg_links.append("Reg" + args[0])
		elif "_KLinkReturn" == func_name:
			if self.return_link != "": sys.exit("last return_link not cleared")
			self.return_link = "Reg" + args[0]
		# like KStore, this doesn't indicate what BB return happened in
		elif "_KReturn" == func_name:
			ret_node = None
			if "RET" not in self.name_to_node:
				ret_node = Node("RET","RET")
			else:
				ret_node = self.name_to_node["RET"]

			src_node = self.name_to_node["Reg" + args[0]]
			self.edges.append((src_node,ret_node))

	 	# ignore function region enter/exit
		elif "_KEnterRegion" == func_name:
			if args[1] != "0":
				region_events.append((args[0],args[1],"enter"))
		elif "_KExitRegion" == func_name:
			if args[1] != "0":
				region_events.append((args[0],args[1],"exit"))
		elif "_KPrepRTable" == func_name:
			for idx in range(int(args[0])):
				node_name = "Reg" + str(idx)
				self.name_to_node[node_name] = Node(node_name,"UNDEF");

class Node:
	def __init__(self,name,type):
		self.name = name
		self.type = type

class CallNode(Node):
	def __init__(self,name,callsite_id):
		self.name = name
		self.type = "CALL"
		self.callsite_id = callsite_id

def main():
	kdump_filename = "main.kdump" # FIXME
	kdump_file = open(kdump_filename,"r")

	curr_func_name = ""
	raw_func_lines = []

	for line in kdump_file:
		if len(line) == 0: continue
		elif "FUNCTION_BEGIN" in line:
			assert len(curr_func_name) == 0, "trying to start new func before ending old one (%s)" % curr_func_name
			curr_func_name = line.split(': ')[1]

		elif "FUNCTION_END" in line:
			end_func_name = line.split(': ')[1]
			assert curr_func_name == end_func_name, "ending function (%s) that isn't current one (%s)" % (end_func_name,curr_func_name)
			if len(raw_func_lines) != 0:
				func = Function(curr_func_name,raw_func_lines)
				func.write_dot_graph()

			# clear out (now obsolete) func name and raw lines
			curr_func_name = ""
			raw_func_lines = []
		else:
			raw_func_lines.append(line)

if __name__ == "__main__":
	main()
