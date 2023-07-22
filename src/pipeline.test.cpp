#include "./pipeline.h"

#include <catch2/catch.hpp>
#include <iostream>
#include <sstream>

// Declare some example components
struct test_sink: ppl::sink<int> {
	const ppl::producer<int>* slot0 = nullptr;

	test_sink() = default;

	auto name() const -> std::string override {
		return "TestSink";
	}

	void connect(const ppl::node* src, int slot) override {
		if (slot == 0) {
			slot0 = dynamic_cast<const ppl::producer<int>*>(src);
		}
	}

	auto poll_next() -> ppl::poll override {
		std::cout << slot0->value() << '\n';
		return ppl::poll::ready;
	}
};

struct test_source: ppl::source<int> {
	int current_value = 0;
	test_source() = default;

	auto name() const -> std::string override {
		return "SimpleSource";
	}

	auto poll_next() -> ppl::poll override {
		if (current_value >= 10)
			return ppl::poll::closed;
		++current_value;
		return ppl::poll::ready;
	}

	auto value() const -> const int& override {
		return current_value;
	}
};

struct test_component: ppl::component<std::tuple<int, int>, int> {
	const ppl::producer<int>* slot0 = nullptr;
	const ppl::producer<int>* slot1 = nullptr;

	int current_value = 0;

	test_component() = default;

	auto name() const -> std::string override {
		return "TestComponent";
	}

	auto poll_next() -> ppl::poll override {
		current_value = slot0->value() + slot1->value();
		return ppl::poll::ready;
	}

	void connect(const ppl::node* src, int slot) override {
		if (slot == 0) {
			slot0 = dynamic_cast<const ppl::producer<int>*>(src);
		} else if (slot == 1) {
			slot1 = dynamic_cast<const ppl::producer<int>*>(src);
		}
	}

	auto value() const -> const int& override {
		return current_value;
	}
};

struct flex_source: ppl::source<int> {
	int current_value = 0;
	int bound;

	explicit flex_source(int bound): bound(bound) {};

	auto name() const -> std::string override {
		return "FlexSource: Bound = " + std::to_string(bound);
	}

	auto poll_next() -> ppl::poll override {
		if (current_value >= bound)
			return ppl::poll::closed;
		++current_value;
		return ppl::poll::ready;
	}

	auto value() const -> const int& override {
		return current_value;
	}
};

struct stream_sink: ppl::sink<int> {
	const ppl::producer<int>* slot0 = nullptr;
	std::stringstream& stream;

	explicit stream_sink(std::stringstream& stream): stream(stream) {};

	auto name() const -> std::string override {
		return "TestSink";
	}

	void connect(const ppl::node* src, int slot) override {
		if (slot == 0) {
			slot0 = dynamic_cast<const ppl::producer<int>*>(src);
		}
	}

	auto poll_next() -> ppl::poll override {
		stream << slot0->value() << ' ';
		return ppl::poll::ready;
	}
};

TEST_CASE("Test Case 1: Test if the concrete_node concept is correctly implemented") {
	// A class without type alias input_type
	struct missing_input: ppl::producer<int> {
		auto name() const -> std::string override {
			return "MissingInput";
		}
		void connect([[maybe_unused]] const node *source, [[maybe_unused]] int slot) override {
			throw ppl::pipeline_error(ppl::pipeline_error_kind::no_such_slot);
		}
		auto poll_next() -> ppl::poll override {
			return ppl::poll::closed;
		}
		auto value() const -> const int& override {
			throw ppl::pipeline_error(ppl::pipeline_error_kind::no_such_slot);
		}
	};

	// A class derived from node but not producer
	struct from_node: ppl::node {
		using input_type [[maybe_unused]] = std::tuple<int>;
		using output_type [[maybe_unused]] = int;
		auto name() const -> std::string override {
			return "FromNode";
		}
		void connect([[maybe_unused]] const node *source, [[maybe_unused]] int slot) override {
			throw ppl::pipeline_error(ppl::pipeline_error_kind::no_such_slot);
		}
		auto poll_next() -> ppl::poll override {
			return ppl::poll::closed;
		}
	};

	// A class with input_type not a tuple
	struct not_tuple: ppl::producer<int> {
		using input_type [[maybe_unused]] = int;
		auto name() const -> std::string override {
			return "FromNode";
		}
		void connect([[maybe_unused]] const node *source, [[maybe_unused]] int slot) override {
			throw ppl::pipeline_error(ppl::pipeline_error_kind::no_such_slot);
		}
		auto poll_next() -> ppl::poll override {
			return ppl::poll::closed;
		}
		auto value() const -> const int& override {
			throw ppl::pipeline_error(ppl::pipeline_error_kind::no_such_slot);
		}
	};

	// A class that is abstract (cannot be constructed)
	struct abstract: ppl::producer<int> {
		using input_type [[maybe_unused]] = int;
		auto name() const -> std::string override {
			return "FromNode";
		}
		void connect([[maybe_unused]] const node *source, [[maybe_unused]] int slot) override {
			throw ppl::pipeline_error(ppl::pipeline_error_kind::no_such_slot);
		}
		auto poll_next() -> ppl::poll override {
			return ppl::poll::closed;
		}
		// Function value() is not overridden
	};

	// A valid class
	struct valid: ppl::producer<int> {
		using input_type [[maybe_unused]] = std::tuple<int, int, int>;
		auto name() const -> std::string override {
			return "FromNode";
		}
		void connect([[maybe_unused]] const node *source, [[maybe_unused]] int slot) override {
			throw ppl::pipeline_error(ppl::pipeline_error_kind::no_such_slot);
		}
		auto poll_next() -> ppl::poll override {
			return ppl::poll::closed;
		}
		auto value() const -> const int& override {
			throw ppl::pipeline_error(ppl::pipeline_error_kind::no_such_slot);
		}
	};

	REQUIRE_FALSE(ppl::concrete_node<missing_input>);
	REQUIRE_FALSE(ppl::concrete_node<from_node>);
	REQUIRE_FALSE(ppl::concrete_node<not_tuple>);
	REQUIRE_FALSE(ppl::concrete_node<abstract>);
	REQUIRE(ppl::concrete_node<valid>);
}

TEST_CASE("Test Case 2: Test if pipeline is not copyable") {
	REQUIRE_FALSE(std::is_copy_constructible_v<ppl::pipeline>);
	REQUIRE_FALSE(std::is_copy_assignable_v<ppl::pipeline>);
}

TEST_CASE("Test Case 3: Test if pipeline is movable") {
	REQUIRE(std::is_move_constructible_v<ppl::pipeline>);
	REQUIRE(std::is_move_assignable_v<ppl::pipeline>);
}

TEST_CASE("Test Case 4: Test if pipeline is default constructible") {
	REQUIRE(std::is_default_constructible_v<ppl::pipeline>);
}

TEST_CASE("Test Case 5: After moving, the original pipeline is empty") {
	ppl::pipeline p;
	const int id = p.create_node<test_sink>();
	REQUIRE(p.get_node(id) != nullptr);

	auto p2 = std::move(p);
	// The new pipeline will maintain all the node originally in the old pipeline
	REQUIRE(p2.get_node(id) != nullptr);
	// The original pipeline should be in an empty state
	REQUIRE(p.get_node(id) == nullptr);

	// And the original pipeline should still be valid
	const int id2 = p.create_node<test_sink>();
	REQUIRE(p.get_node(id2) != nullptr);
}

TEST_CASE("Test Case 6: In moving, self-assignment has no effect") {
	ppl::pipeline p;
	const int id = p.create_node<test_sink>();
	REQUIRE(p.get_node(id) != nullptr);

	p = std::move(p);
	REQUIRE(p.get_node(id) != nullptr);
}

TEST_CASE("Test Case 7: Test create_node()") {
	ppl::pipeline p;
	const int id = p.create_node<test_sink>();
	// Test if the node is created
	REQUIRE(p.get_node(id) != nullptr);
	// The newly created node should not have dependencies
	REQUIRE(p.get_dependencies(id).empty());
}

TEST_CASE("Test Case 8: Test successful connect()") {
	ppl::pipeline p;
	const int sink = p.create_node<test_sink>();
	const int source = p.create_node<test_source>();
	// Test if the connection is successful
	REQUIRE_NOTHROW(p.connect(source, sink, 0));
	// The dependencies_ vector of src node should be updated
	// But the dependencies_ vector of dst node should not keep unchanged
	REQUIRE(p.get_dependencies(sink).empty());
	REQUIRE(p.get_dependencies(source).size() == 1);
}

TEST_CASE("Test Case 9: Test failed connect()") {
	ppl::pipeline p;
	const int sink = p.create_node<test_sink>();
	const int source = p.create_node<test_source>();
	REQUIRE_NOTHROW(p.connect(source, sink, 0));
	// Test if the connection will be rejected if one node is invalid, and if a proper exception will be thrown
	try {
		p.connect(source + 1, sink, 0);
		REQUIRE(false);
	} catch (ppl::pipeline_error &e) {
		REQUIRE(e.kind() == ppl::pipeline_error_kind::invalid_node_id);
	}

	// Test if the connection will be rejected if the slot to be connected is already full,
	// and if a proper exception will be thrown
	try {
		p.connect(source, sink, 0);
		REQUIRE(false);
	} catch (ppl::pipeline_error &e) {
		REQUIRE(e.kind() == ppl::pipeline_error_kind::slot_already_used);
	}

	// Test if the connection will be rejected if the slot does not exist, and if a proper exception will be thrown
	try {
		p.connect(source, sink, 1);
		REQUIRE(false);
	} catch (ppl::pipeline_error &e) {
		REQUIRE(e.kind() == ppl::pipeline_error_kind::no_such_slot);
	}

	struct test_sink_double: ppl::sink<double> {
		const ppl::producer<double>* slot0 = nullptr;

		test_sink_double() = default;

		auto name() const -> std::string override {
			return "TestSinkDouble";
		}

		void connect(const ppl::node* src, int slot) override {
			if (slot == 0) {
				slot0 = dynamic_cast<const ppl::producer<double>*>(src);
			}
		}

		auto poll_next() -> ppl::poll override {
			std::cout << slot0->value() << '\n';
			return ppl::poll::ready;
		}
	};

	// Test if the connection will be rejected if the output type do not match the input type,
	// and if a proper exception will be thrown
	const int sink_double = p.create_node<test_sink_double>();
	try {
		p.connect(source, sink_double, 0);
		REQUIRE(false);
	} catch (ppl::pipeline_error &e) {
		REQUIRE(e.kind() == ppl::pipeline_error_kind::connection_type_mismatch);
	}
}

TEST_CASE("Test Case 10: Test successful disconnect()") {
	ppl::pipeline p;
	const int sink = p.create_node<test_sink>();
	const int source1 = p.create_node<test_source>();
	const int source2 = p.create_node<test_source>();
	const int component = p.create_node<test_component>();
	REQUIRE_NOTHROW(p.connect(source1, component, 0));
	REQUIRE_NOTHROW(p.connect(source2, component, 1));
	REQUIRE_NOTHROW(p.connect(component, sink, 0));

	// Disconnect source1 and component
	REQUIRE_NOTHROW(p.disconnect(source1, component));
	// The dependencies_ vector of source1 should be updated
	REQUIRE(p.get_dependencies(source1).empty());
	// The connections_ map of component should be updated, so slot0 of component should be empty and
	// can be connected by another node
	REQUIRE_NOTHROW(p.connect(source2, component, 0));
}

TEST_CASE("Test Case 11: Test failed disconnect()") {
	ppl::pipeline p;
	const int sink = p.create_node<test_sink>();
	const int source1 = p.create_node<test_source>();
	const int source2 = p.create_node<test_source>();
	const int component = p.create_node<test_component>();
	REQUIRE_NOTHROW(p.connect(source1, component, 0));
	REQUIRE_NOTHROW(p.connect(source2, component, 1));
	REQUIRE_NOTHROW(p.connect(component, sink, 0));

	// Test if the disconnection will be rejected if one node is invalid, and if a proper exception will be thrown
	try {
		p.disconnect(5, component);
		REQUIRE(false);
	} catch (ppl::pipeline_error &e) {
		REQUIRE(e.kind() == ppl::pipeline_error_kind::invalid_node_id);
	}

	// Test there should be nothing happen if these two nodes are not connected
	REQUIRE_NOTHROW(p.disconnect(source1, sink));
	// After "disconnection", the pipeline should still in valid state
	REQUIRE(p.is_valid());
}

TEST_CASE("Test Case 12: Test successful erase()") {
	ppl::pipeline p;
	const int sink = p.create_node<test_sink>();
	const int source1 = p.create_node<test_source>();
	const int source2 = p.create_node<test_source>();
	const int component = p.create_node<test_component>();
	REQUIRE_NOTHROW(p.connect(source1, component, 0));
	REQUIRE_NOTHROW(p.connect(source2, component, 1));
	REQUIRE_NOTHROW(p.connect(component, sink, 0));

	// Erase component
	REQUIRE_NOTHROW(p.erase_node(component));
	// Node component should be removed from pipeline
	// And the memory it used should be cleaned up, but we can't check this...
	REQUIRE(p.get_node(component) == nullptr);
	// The dependencies_ vector of source1 and source2 should be updated
	REQUIRE(p.get_dependencies(source1).empty());
	REQUIRE(p.get_dependencies(source2).empty());
	// The connections_ map of sink should be updated, so slot0 of sink should be empty and can be connected to
	// another node
	REQUIRE_NOTHROW(p.connect(source1, sink, 0));
}

TEST_CASE("Test Case 13: Test failed erase()") {
	ppl::pipeline p;
	const int sink = p.create_node<test_sink>();
	const int source1 = p.create_node<test_source>();
	const int source2 = p.create_node<test_source>();
	const int component = p.create_node<test_component>();
	REQUIRE_NOTHROW(p.connect(source1, component, 0));
	REQUIRE_NOTHROW(p.connect(source2, component, 1));
	REQUIRE_NOTHROW(p.connect(component, sink, 0));

	// Test  if erase will be rejected if the node is invalid, and if a proper exception will be thrown
	try {
		p.erase_node(component + 1);
		REQUIRE(false);
	} catch (ppl::pipeline_error &e) {
		REQUIRE(e.kind() == ppl::pipeline_error_kind::invalid_node_id);
	}
}

TEST_CASE("Test Case 14: is_valid() should return false when some slots are not filled") {
	ppl::pipeline p;
	const int sink = p.create_node<test_sink>();
	const int source1 = p.create_node<test_source>();
	const int component = p.create_node<test_component>();
	// Slot1 of component is not filled
	REQUIRE_NOTHROW(p.connect(source1, component, 0));
	REQUIRE_NOTHROW(p.connect(component, sink, 0));

	REQUIRE_FALSE(p.is_valid());
}

TEST_CASE("Test Case 15: is_valid() should return false when some nodes have no dependency") {
	ppl::pipeline p;
	const int sink = p.create_node<test_sink>();
	const int source1 = p.create_node<test_source>();
	const int source2 [[maybe_unused]] = p.create_node<test_source>();
	const int component = p.create_node<test_component>();
	// source2 has no dependency since it is not connected to any other node
	REQUIRE_NOTHROW(p.connect(source1, component, 0));
	REQUIRE_NOTHROW(p.connect(source1, component, 1));
	REQUIRE_NOTHROW(p.connect(component, sink, 0));

	REQUIRE_FALSE(p.is_valid());
}

TEST_CASE("Test Case 16: is_valid() should return false when there is no source node or no sink node") {
	ppl::pipeline p;
	REQUIRE_FALSE(p.is_valid());
}

TEST_CASE("Test Case 17: is_valid() should return false if there is a sub pipeline") {
	ppl::pipeline p;
	const int sink = p.create_node<test_sink>();
	const int source1 = p.create_node<test_source>();
	const int source2 = p.create_node<test_source>();
	const int component = p.create_node<test_component>();

	const int sink2 = p.create_node<test_sink>();
	const int source3 = p.create_node<test_source>();

	// Construct a pipeline in pipeline p
	REQUIRE_NOTHROW(p.connect(source1, component, 0));
	REQUIRE_NOTHROW(p.connect(source2, component, 1));
	REQUIRE_NOTHROW(p.connect(component, sink, 0));

	// Construct another pipeline in pipeline p, and make it not connected to the first pipeline
	REQUIRE_NOTHROW(p.connect(source3, sink2, 0));

	// A sub pipeline should be detected
	REQUIRE_FALSE(p.is_valid());
}

TEST_CASE("Test Case 18: is_valid() should return false if there is a cycle") {
	ppl::pipeline p;
	const int sink [[maybe_unused]] = p.create_node<test_sink>();
	const int source1 = p.create_node<test_source>();
	const int source2 = p.create_node<test_source>();
	const int source3 = p.create_node<test_source>();
	const int component1 = p.create_node<test_component>();
	const int component2 = p.create_node<test_component>();
	const int component3 = p.create_node<test_component>();

	// Construct a cycle
	REQUIRE_NOTHROW(p.connect(source1, component1, 0));
	REQUIRE_NOTHROW(p.connect(source2, component2, 0));
	REQUIRE_NOTHROW(p.connect(source3, component3, 0));
	REQUIRE_NOTHROW(p.connect(component1, component2, 1));
	REQUIRE_NOTHROW(p.connect(component2, component3, 1));
	REQUIRE_NOTHROW(p.connect(component3, component1, 1));
	REQUIRE_NOTHROW(p.connect(component3, sink, 0));

	// A cycle should be detected
	REQUIRE_FALSE(p.is_valid());
}

TEST_CASE("Test Case 19: is_valid() should return true if it is a valid pipeline") {
	ppl::pipeline p;
	const int sink [[maybe_unused]] = p.create_node<test_sink>();
	const int source1 = p.create_node<test_source>();
	const int source2 = p.create_node<test_source>();
	const int source3 = p.create_node<test_source>();
	const int source4 = p.create_node<test_source>();
	const int component1 = p.create_node<test_component>();
	const int component2 = p.create_node<test_component>();
	const int component3 = p.create_node<test_component>();

	// Construct a valid pipeline
	REQUIRE_NOTHROW(p.connect(source1, component1, 0));
	REQUIRE_NOTHROW(p.connect(source2, component1, 1));
	REQUIRE_NOTHROW(p.connect(source3, component2, 0));
	REQUIRE_NOTHROW(p.connect(component1, component2, 1));
	REQUIRE_NOTHROW(p.connect(component2, component3, 0));
	REQUIRE_NOTHROW(p.connect(source4, component3, 1));
	REQUIRE_NOTHROW(p.connect(component3, sink, 0));

	REQUIRE(p.is_valid());
}

TEST_CASE("Test Case 20: In step(), if one node is closed, than all the nodes depend on it should be closed") {
	ppl::pipeline p;
	// Construct a source with bound 5, which means that when it is polled 5 times,
	// it will be closed at the 6th polling
	const int source1 = p.create_node<flex_source>(5);
	// Construct a source with bound 10, which means that when it is polled 10 times,
	// it will be closed at the 11th polling
	const int source2 = p.create_node<flex_source>(10);

	const int component = p.create_node<test_component>();

	std::stringstream stream1;
	std::stringstream stream2;
	const int sink1 = p.create_node<stream_sink>(stream1);
	const int sink2 = p.create_node<stream_sink>(stream2);

	REQUIRE_NOTHROW(p.connect(source1, component, 0));
	REQUIRE_NOTHROW(p.connect(source2, component, 1));
	REQUIRE_NOTHROW(p.connect(component, sink1, 0));
	REQUIRE_NOTHROW(p.connect(source2, sink2, 0));

	REQUIRE(p.is_valid());

	// After 5 steps, source1 reaches its bound
	REQUIRE_FALSE(p.step());
	REQUIRE_FALSE(p.step());
	REQUIRE_FALSE(p.step());
	REQUIRE_FALSE(p.step());
	REQUIRE_FALSE(p.step());

	// At step 6, source1 should be closed, and the sink1 and component should also be closed. But source2 should not be closed, and
	// sink2 should not be closed, since sink 2 is not depend on source 1 nor component
	// After 10 steps, source2 reaches its bound
	REQUIRE_FALSE(p.step());
	REQUIRE_FALSE(p.step());
	REQUIRE_FALSE(p.step());
	REQUIRE_FALSE(p.step());
	REQUIRE_FALSE(p.step());

	// At step 11, source2 should be closed, and the sink2 should also be closed, step() should return true since
	// all the sink nodes are closed
	REQUIRE(p.step());

	// Sink1 should be polled 5 times and then closed
	REQUIRE(stream1.str() == "2 4 6 8 10 ");

	// Sink2 should be polled 10 times and then closed
	REQUIRE(stream2.str() == "1 2 3 4 5 6 7 8 9 10 ");
}

struct skip_source: ppl::source<int> {
	int current_value = 0;
	int bound;

	explicit skip_source(int bound): bound(bound) {};

	auto name() const -> std::string override {
		return "SkipSource: Bound = " + std::to_string(bound);
	}

	auto poll_next() -> ppl::poll override {
		if (current_value >= bound)
			return ppl::poll::closed;
		if (current_value % 2 == 0) {
			++current_value;
			return ppl::poll::empty;
		}
		++current_value;
		return ppl::poll::ready;
	}

	auto value() const -> const int& override {
		return current_value;
	}
};

TEST_CASE("Test Case 21: In step(), if one node is empty, then all the node depend on it should be skipped") {
	ppl::pipeline p;
	// Source1 is a skip source with bound 6, which means this source will return empty when current_value is even
	const int source1 = p.create_node<skip_source>(6);
	const int source2 = p.create_node<flex_source>(10);
	const int component = p.create_node<test_component>();

	std::stringstream stream1;
	std::stringstream stream2;
	const int sink1 = p.create_node<stream_sink>(stream1);
	const int sink2 = p.create_node<stream_sink>(stream2);

	REQUIRE_NOTHROW(p.connect(source1, component, 0));
	REQUIRE_NOTHROW(p.connect(source2, component, 1));
	REQUIRE_NOTHROW(p.connect(component, sink1, 0));
	REQUIRE_NOTHROW(p.connect(source2, sink2, 0));

	REQUIRE(p.is_valid());

	// After 6 steps, source1 reaches its bound
	// And at step 1, 3 and 5, source1 will return empty so sink1 and component will be skipped at these steps
	// But source2 and sink2 should not be skipped since they are not depend on source1 nor component
	REQUIRE_FALSE(p.step());
	REQUIRE_FALSE(p.step());
	REQUIRE_FALSE(p.step());
	REQUIRE_FALSE(p.step());
	REQUIRE_FALSE(p.step());
	REQUIRE_FALSE(p.step());

	// After 10 steps, source2 reaches its bound
	REQUIRE_FALSE(p.step());
	REQUIRE_FALSE(p.step());
	REQUIRE_FALSE(p.step());
	REQUIRE_FALSE(p.step());

	// At step 11, sink2 should be closed, since sink1 was closed at step 7, step() should return true
	REQUIRE(p.step());

	REQUIRE(stream1.str() == "4 8 12 ");
	REQUIRE(stream2.str() == "1 2 3 4 5 6 7 8 9 10 ");
}

TEST_CASE("Test Case 22: In step(), if one node is closed because of dependency, then it can be reopened when the "
          "dependency is/are replaced") {
	ppl::pipeline p;
	// Construct a source with bound 5, which means that when it is polled 5 times,
	// it will be closed at the 6th polling
	const int source1 = p.create_node<flex_source>(5);
	// Construct a source with bound 10, which means that when it is polled 10 times,
	// it will be closed at the 11th polling
	const int source2 = p.create_node<flex_source>(10);

	const int component = p.create_node<test_component>();

	std::stringstream stream1;
	std::stringstream stream2;
	const int sink1 = p.create_node<stream_sink>(stream1);
	const int sink2 = p.create_node<stream_sink>(stream2);

	REQUIRE_NOTHROW(p.connect(source1, component, 0));
	REQUIRE_NOTHROW(p.connect(source2, component, 1));
	REQUIRE_NOTHROW(p.connect(component, sink1, 0));
	REQUIRE_NOTHROW(p.connect(source2, sink2, 0));

	REQUIRE(p.is_valid());

	// After 5 steps, source1 reaches its bound
	REQUIRE_FALSE(p.step());
	REQUIRE_FALSE(p.step());
	REQUIRE_FALSE(p.step());
	REQUIRE_FALSE(p.step());
	REQUIRE_FALSE(p.step());

	// At step 6, source1 should be closed, and the sink1 and component should also be closed. But source2 should not
	// be closed, and sink2 should not be closed, since sink 2 is not depend on source 1 nor component
	// After 10 steps, source2 reaches its bound
	REQUIRE_FALSE(p.step());
	// Replace the closed source by a new source
	const int source3 = p.create_node<flex_source>(5);
	// Delete the closed node
	REQUIRE_NOTHROW(p.erase_node(source1));
	REQUIRE_NOTHROW(p.connect(source3, component, 0));
	// pipeline should still be valid after replacement
	REQUIRE(p.is_valid());

	// Now the pipeline can be polled for 4 more times, since source2 has a bound of 10.
	// And component and sink1 should be reopened and output more values
	REQUIRE_FALSE(p.step());
	REQUIRE_FALSE(p.step());
	REQUIRE_FALSE(p.step());
	REQUIRE_FALSE(p.step());

	// At step 11, source2 is closed, it will make component, sink1 and sink2 all closed, so step() should return true
	REQUIRE(p.step());
	// Sink1 should produce 4 more values
	REQUIRE(stream1.str() == "2 4 6 8 10 8 10 12 14 ");
	REQUIRE(stream2.str() == "1 2 3 4 5 6 7 8 9 10 ");

}

TEST_CASE("Test Case 23: Test that run() can perform step() continuously until all the sink nodes are closed") {
	ppl::pipeline p;
	const int source1 = p.create_node<flex_source>(5);
	const int source2 = p.create_node<flex_source>(10);

	const int component = p.create_node<test_component>();

	std::stringstream stream1;
	std::stringstream stream2;
	const int sink1 = p.create_node<stream_sink>(stream1);
	const int sink2 = p.create_node<stream_sink>(stream2);

	REQUIRE_NOTHROW(p.connect(source1, component, 0));
	REQUIRE_NOTHROW(p.connect(source2, component, 1));
	REQUIRE_NOTHROW(p.connect(component, sink1, 0));
	REQUIRE_NOTHROW(p.connect(source2, sink2, 0));

	REQUIRE(p.is_valid());

	p.run();

	REQUIRE(stream1.str() == "2 4 6 8 10 ");
	REQUIRE(stream2.str() == "1 2 3 4 5 6 7 8 9 10 ");
}

TEST_CASE("Test Case 24: Test that run() can perform step() continuously until all the sink nodes are closed") {
	ppl::pipeline p;
	const int source1 = p.create_node<skip_source>(6);
	const int source2 = p.create_node<flex_source>(10);
	const int component = p.create_node<test_component>();

	std::stringstream stream1;
	std::stringstream stream2;
	const int sink1 = p.create_node<stream_sink>(stream1);
	const int sink2 = p.create_node<stream_sink>(stream2);

	REQUIRE_NOTHROW(p.connect(source1, component, 0));
	REQUIRE_NOTHROW(p.connect(source2, component, 1));
	REQUIRE_NOTHROW(p.connect(component, sink1, 0));
	REQUIRE_NOTHROW(p.connect(source2, sink2, 0));

	REQUIRE(p.is_valid());

	p.run();

	REQUIRE(stream1.str() == "4 8 12 ");
	REQUIRE(stream2.str() == "1 2 3 4 5 6 7 8 9 10 ");
}

TEST_CASE("Test Case 25: Test that run() can be called multiple times, if we replace all the closed source nodes"
          "with new source nodes") {
	ppl::pipeline p;
	const int source1 = p.create_node<skip_source>(6);
	const int source2 = p.create_node<flex_source>(10);
	const int component = p.create_node<test_component>();

	std::stringstream stream1;
	std::stringstream stream2;
	const int sink1 = p.create_node<stream_sink>(stream1);
	const int sink2 = p.create_node<stream_sink>(stream2);

	REQUIRE_NOTHROW(p.connect(source1, component, 0));
	REQUIRE_NOTHROW(p.connect(source2, component, 1));
	REQUIRE_NOTHROW(p.connect(component, sink1, 0));
	REQUIRE_NOTHROW(p.connect(source2, sink2, 0));

	REQUIRE(p.is_valid());

	p.run();

	REQUIRE(stream1.str() == "4 8 12 ");
	REQUIRE(stream2.str() == "1 2 3 4 5 6 7 8 9 10 ");

	// Delete all the closed source
	REQUIRE_NOTHROW(p.erase_node(source1));
	REQUIRE_NOTHROW(p.erase_node(source2));
	// Create two new source
	const int source3 = p.create_node<skip_source>(6);
	const int source4 = p.create_node<flex_source>(10);
	// Connect the new source to the pipeline
	REQUIRE_NOTHROW(p.connect(source3, component, 0));
	REQUIRE_NOTHROW(p.connect(source4, component, 1));
	REQUIRE_NOTHROW(p.connect(source4, sink2, 0));
	// Pipeline should still be valid after replacement
	REQUIRE(p.is_valid());

	p.run();

	// Sink1 and sink2 should produce more values
	REQUIRE(stream1.str() == "4 8 12 4 8 12 ");
	REQUIRE(stream2.str() == "1 2 3 4 5 6 7 8 9 10 1 2 3 4 5 6 7 8 9 10 ");
}

TEST_CASE("Test Case 26: Test << operator can be used to print the information of the pipeline and all things "
          "are in order") {
	ppl::pipeline p;
	const int source1 = p.create_node<skip_source>(6);
	const int source2 = p.create_node<flex_source>(10);
	const int component = p.create_node<test_component>();

	std::stringstream stream1;
	std::stringstream stream2;
	const int sink1 = p.create_node<stream_sink>(stream1);
	const int sink2 = p.create_node<stream_sink>(stream2);

	REQUIRE_NOTHROW(p.connect(source1, component, 0));
	REQUIRE_NOTHROW(p.connect(source2, sink2, 0));
	REQUIRE_NOTHROW(p.connect(source2, component, 1));
	REQUIRE_NOTHROW(p.connect(component, sink1, 0));

	REQUIRE(p.is_valid());

	std::stringstream out;
	out << p;
	REQUIRE(out.str() == "digraph G {\n"
						 "  \"1 SkipSource: Bound = 6\"\n"
						 "  \"2 FlexSource: Bound = 10\"\n"
						 "  \"3 TestComponent\"\n"
						 "  \"4 TestSink\"\n"
						 "  \"5 TestSink\"\n"
						 "\n"
						 "  \"1 SkipSource: Bound = 6\" -> \"3 TestComponent\"\n"
						 "  \"2 FlexSource: Bound = 10\" -> \"3 TestComponent\"\n"
						 "  \"2 FlexSource: Bound = 10\" -> \"5 TestSink\"\n"
						 "  \"3 TestComponent\" -> \"4 TestSink\"\n"
						 "}\n");
}

TEST_CASE("Test Case 27: Test << operator produce duplicated lines if one source node is connected to one component "
          "multiple times on different slots") {
	ppl::pipeline p;
	const int source1 = p.create_node<skip_source>(6);
	const int component = p.create_node<test_component>();

	std::stringstream stream1;
	const int sink1 = p.create_node<stream_sink>(stream1);

	REQUIRE_NOTHROW(p.connect(source1, component, 0));
	REQUIRE_NOTHROW(p.connect(source1, component, 1));
	REQUIRE_NOTHROW(p.connect(component, sink1, 0));

	REQUIRE(p.is_valid());

	std::stringstream out;
	out << p;
	REQUIRE(out.str() == "digraph G {\n"
						 "  \"1 SkipSource: Bound = 6\"\n"
						 "  \"2 TestComponent\"\n"
						 "  \"3 TestSink\"\n"
						 "\n"
	                     // Duplicate lines
						 "  \"1 SkipSource: Bound = 6\" -> \"2 TestComponent\"\n"
						 "  \"1 SkipSource: Bound = 6\" -> \"2 TestComponent\"\n"
						 "  \"2 TestComponent\" -> \"3 TestSink\"\n"
						 "}\n");
}
