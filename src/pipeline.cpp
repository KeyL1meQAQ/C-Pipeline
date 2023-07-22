#include "./pipeline.h"
#include <iomanip>
#include <iostream>

/**
 * Exceptions
 */
namespace ppl {
	auto pipeline_error::kind() -> pipeline_error_kind {
		return kind_;
	}
	const char* pipeline_error::what() const noexcept {
		switch (kind_) {
			case pipeline_error_kind::invalid_node_id:
				return "invalid node ID";
			case pipeline_error_kind::no_such_slot:
				return "no such slot";
			case pipeline_error_kind::slot_already_used:
				return "slot already used";
			case pipeline_error_kind::connection_type_mismatch:
				return "connection type mismatch";
		    default:
			    return "unknown pipeline error";
		}
	}
}

/**
 * Pipeline
 */
namespace ppl {
	pipeline::pipeline(pipeline&& other) noexcept {
		nodes_ = std::move(other.nodes_);
		other.nodes_.clear();
	}
	auto pipeline::operator=(pipeline&& other) noexcept -> pipeline& {
		if (this != &other) {
			nodes_ = std::move(other.nodes_);
			other.nodes_.clear();
		}
		return *this;
	}
	pipeline::~pipeline() {
		for (auto& [id, node]: nodes_) {
			delete node;
		}
	}
	void pipeline::erase_node(pipeline::node_id n_id) {
		auto node = get_node(n_id);
		if (node == nullptr) {
			throw pipeline_error(pipeline_error_kind::invalid_node_id);
		}
		// Update the dependencies list of the node that connected to this node
		for (auto &[slot, src]: node->connections_) {
			std::erase_if(get_node(src)->dependencies_, [n_id](auto& item) {
				return item.first == n_id;
			});
		}
		// Update the connections list of the nodes that this node connected to
		for (auto &[dst, slot]: node->dependencies_) {
			get_node(dst)->connections_.erase(slot);
		}

		// Delete this node
		delete node;
		nodes_.erase(n_id);
	}
	auto pipeline::get_node(pipeline::node_id n_id) const noexcept -> node* {
		auto it = nodes_.find(n_id);
		if (it == nodes_.end()) {
			return nullptr;
		}
		return it->second;
	}
	void pipeline::connect(pipeline::node_id src, pipeline::node_id dst, int slot) const {
		auto src_node = get_node(src);
		auto dst_node = get_node(dst);

		// Check if the both nodes exist
		if (src_node == nullptr || dst_node == nullptr) {
			throw pipeline_error(pipeline_error_kind::invalid_node_id);
		}
		// Check if the target slot is already in use
		if (dst_node->connections_.find(slot) != dst_node->connections_.end()) {
			throw pipeline_error(pipeline_error_kind::slot_already_used);
		}
		// Check if the slot is existed
		if (slot < 0 || static_cast<unsigned long>(slot) >= dst_node->get_input_types().size()) {
			throw pipeline_error(pipeline_error_kind::no_such_slot);
		}
		// Check if the output type of the source node matches the input type of the target node on target slot
		if (dst_node->get_input_types().at(static_cast<unsigned long>(slot)) != src_node->get_output_type()) {
			throw pipeline_error(pipeline_error_kind::connection_type_mismatch);
		}
		dst_node->connect(src_node, slot);
		dst_node->connections_.emplace(slot, src);
		src_node->dependencies_.emplace_back(dst, slot);
	}
	void pipeline::disconnect(pipeline::node_id src, pipeline::node_id dst) const {
		auto src_node = get_node(src);
		auto dst_node = get_node(dst);

		if (src_node == nullptr || dst_node == nullptr) {
			throw pipeline_error(pipeline_error_kind::invalid_node_id);
		}
		for (auto &[slot, connected_by]: dst_node->connections_) {
			if (src == connected_by) {
				dst_node->connect(nullptr, slot);
			}
		}
		std::erase_if(dst_node->connections_, [src](const auto& item) {
			return item.second == src;
		});
		std::erase_if(src_node->dependencies_, [dst](const auto& item) {
			return item.first == dst;
		});
	}
	auto pipeline::get_dependencies(pipeline::node_id src) const -> const std::vector<std::pair<node_id, int>> {
		auto src_node = get_node(src);
		if (src_node == nullptr) {
			throw pipeline_error(pipeline_error_kind::invalid_node_id);
		}
		return src_node->dependencies_;
	}
	auto pipeline::is_valid() const noexcept -> bool {
		bool has_sink = false;
		bool has_source = false;
		for (const auto [id, node]: nodes_) {
			if (node->connections_.size() != node->get_input_types().size()) {
				return false;
			}
			if (node->get_output_type() != typeid(void) && get_dependencies(id).empty()) {
				return false;
			}
			if (node->get_output_type() == typeid(void)) {
				has_sink = true;
			}
			if (node->get_input_types().empty()) {
				has_source = true;
			}
		}
		if (!has_sink || !has_source) {
			return false;
		}

		const auto& has_cycle = [this](const int src, auto& visited, auto&& has_cycle) -> bool {
			// 1 = visiting, 2 = visited
			// If this node is marked as visiting, then there is a cycle
			if (visited[src] == 1) {
				return true;
			}
			// If this node is marked as visited, then we don't need to visit it again, and there is no cycle
			if (visited[src] == 2) {
				return false;
			}
			// Mark the current node as visiting
			visited[src] = 1;
			const node* node = get_node(src);
			// Traverse all the node inserted in the slots of current node
			for (const auto& [slot, next_src]: node->connections_) {
				if (has_cycle(next_src, visited, has_cycle)) {
					return true;
				}
			}
			// If we have visited all the nodes connected to this node, then mark it as visited
			visited[src] = 2;
			return false;
		};

		std::unordered_map<node_id, int> visited;

		// Check if there is a cycle in the pipeline, using DFS
		for (const auto [id, node]: nodes_) {
			// Start at all sink nodes
			if (node->get_output_type() == typeid(void)) {
				if (has_cycle(id, visited, has_cycle)) {
					return false;
				}
			}
		}

		// Find if all the nodes can be reached from any other node, if not, than there is a sub pipeline
		const auto& dfs_all = [this](const int src, auto& visited, auto&& dfs_all) -> void {
			if (visited[src] == 1) {
				return;
			}
			visited[src] = 1;
			const node* node = get_node(src);
			// Regard this DAG as an undirected graph, and traverse all the nodes next to it
			// (both in connections and dependencies)
			for (const auto& [slot, next_src]: node->connections_) {
				dfs_all(next_src, visited, dfs_all);
			}
			for (const auto& [next_src, slot]: node->dependencies_) {
				dfs_all(next_src, visited, dfs_all);
			}
		};

		std::unordered_map<node_id, int> visited_all;

		// Check if there is a sub pipeline in the pipeline, using DFS
		for (const auto [id, node]: nodes_) {
			// Start from any node in nodes_ map
			// And we only do traverse once, if the graph is connected, then all the nodes will be visited
			// in 1 traverse
			dfs_all(id, visited_all, dfs_all);
			if (visited_all.size() != nodes_.size()) {
				return false;
			}
			break;
		}
		return true;
	}
	auto pipeline::step() const noexcept -> bool {
		const auto& polling = [this](const int src, auto& visited, auto&& polling) -> ppl::poll {
			if (visited.contains(src)) {
				return visited[src];
			}

			node* node = get_node(src);
			for (const auto& [slot, next_src]: node->connections_) {
				auto res = polling(next_src, visited, polling);
				if (res == poll::empty) {
					visited[src] = ppl::poll::empty;
					return poll::empty;
				}
				if (res == poll::closed) {
					visited[src] = poll::closed;
					return poll::closed;
				}
			}
			visited[src] = node->poll_next();
			return visited[src];
		};

		std::unordered_map<node_id, ppl::poll> visited;

		bool is_all_closed = true;
		for (const auto& [id, node]: nodes_) {
			if (node->get_output_type() == typeid(void)) {
				if (polling(id, visited, polling) != poll::closed) {
					is_all_closed = false;
				}
			}
		}

		return is_all_closed;
	}
	void pipeline::run() const noexcept {
		while (!step()) {}
	}
	std::ostream& operator<<(std::ostream& ostream, const pipeline& pipeline) {
		ostream << "digraph G {\n";
		for (auto& [node_id, node]: pipeline.nodes_) {
			auto ss = std::stringstream();
			ss << node_id << " " << node->name();
			ostream << "  " << std::quoted(ss.str()) << "\n";
		}
		ostream << "\n";

		for (auto& [node_id, node]: pipeline.nodes_) {
			auto dependencies = pipeline.get_dependencies(node_id);
			std::sort(dependencies.begin(), dependencies.end(), [](const auto& a, const auto& b) {
				return a.first < b.first;
			});
			for (auto& item: dependencies) {
				auto start = std::stringstream();
				start << node_id << " " << node->name();
				auto end = std::stringstream();
				end << item.first << " " << pipeline.get_node(item.first)->name();
				ostream << "  " << std::quoted(start.str()) << " -> " << std::quoted(end.str()) << "\n";
			}
		}
		ostream << "}\n";
		return ostream;
	}

}