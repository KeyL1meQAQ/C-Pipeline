#ifndef COMP6771_PIPELINE_H
#define COMP6771_PIPELINE_H

#include <exception>
#include <string>
#include <unordered_map>
#include <map>
#include <vector>
#include <typeindex>

namespace ppl {
	// Errors that may occur in a pipeline.
	enum class pipeline_error_kind {
		// An expired node ID was provided.
		invalid_node_id,
		// Attempting to bind a non-existant slot.
		no_such_slot,
		// Attempting to bind to a slot that is already filled.
		slot_already_used,
		// The output type and input types for a connection don't match.
		connection_type_mismatch,
	};

	struct pipeline_error: std::exception {
		explicit pipeline_error(pipeline_error_kind kind): kind_(kind) {};
		auto kind() -> pipeline_error_kind;
		[[nodiscard]] const char * what() const noexcept override;

	 private:
		pipeline_error_kind kind_;
	};

	// The result of a poll_next() operation.
	enum class poll {
		// A value is available.
		ready,
		// No value is available this time, but there might be one later.
		empty,
		// No value is available, and there never will be again:
		// every future poll for this node will return `poll::closed` again.
		closed,
	};

	class node {
	 public:
		[[nodiscard]] virtual auto name() const -> std::string = 0;
		virtual ~node() = default;

	 private:
		[[nodiscard]] virtual auto poll_next() -> poll = 0;
		virtual void connect(const node* source, int slot) = 0;

		// You may add any other virtual functions you feel you may want here.
		using slot_id = int;
		std::unordered_map<slot_id, const int> connections_;
		std::vector<std::pair<int, int>> dependencies_;

		virtual auto get_input_types() const noexcept -> std::vector<std::type_index> {
			return {};
		}
		virtual auto get_output_type() const noexcept -> std::type_index {
			return typeid(void);
		}

		friend class pipeline;
	};

	template <typename Output>
	struct producer: node {
		using output_type = Output;
		virtual auto value() const -> const output_type& = 0; // only when `Output` is not `void`

	 private:
		auto get_output_type() const noexcept -> std::type_index override {
			return typeid(Output);
		}
	};

	template <>
	struct producer<void>: node {
		using output_type = void;
	};

	/**
	 * Helper Functions
	 */
	namespace internal {
		template<typename Tuple, std::size_t... Indexes>
		auto get_types(const Tuple&, std::index_sequence<Indexes...>) noexcept -> std::vector<std::type_index> {
			return {typeid(std::tuple_element_t<Indexes, Tuple>)...};
		}
		template <typename T>
		struct is_tuple: std::false_type {};
		template <typename... Ts>
		struct is_tuple<std::tuple<Ts...>>: std::true_type {};

	}

	template <typename Input, typename Output>
	struct component: producer<Output> {
		using input_type = Input;

	 private:
		[[nodiscard]] auto get_input_types() const noexcept -> std::vector<std::type_index> override {
			return internal::get_types(input_type{}, std::make_index_sequence<std::tuple_size_v<input_type>>{});
		}
		[[nodiscard]] auto get_output_type() const noexcept -> std::type_index override {
			return typeid(Output);
		};
	};

	template <typename Input>
	struct sink: component<std::tuple<Input>, void> {};

	template <typename Output>
	struct source: component<std::tuple<>, Output> {
	 private:
		void connect([[maybe_unused]] const node *source, [[maybe_unused]] int slot) override {
		    throw pipeline_error(pipeline_error_kind::no_such_slot);
		};
	};

	// The requirements that a type `N` must satisfy
	// to be used as a component in a pipeline.
	template <typename N>
	// 3.6.0
	concept concrete_node = requires(N n) {
		typename N::input_type;
		typename N::output_type;
	} and internal::is_tuple<typename N::input_type>::value
		and std::is_base_of_v<producer<typename N::output_type>, N>;

	class pipeline {
	 public:
		// 3.6.1
		using node_id = int;

		// 3.6.2
		pipeline(): nodes_(), current_id(1) {};
		pipeline(const pipeline &) = delete;
		pipeline(pipeline&&) noexcept;
		auto operator=(const pipeline &) -> pipeline& = delete;
		auto operator=(pipeline &&) noexcept -> pipeline&;
		~pipeline();

		// 3.6.3
		template <typename N, typename... Args>
		requires concrete_node<N> and std::constructible_from<N, Args...>
		auto create_node(Args&& ...args) noexcept -> node_id {
			nodes_.emplace(current_id, new N(std::forward<Args>(args)...));
			return current_id++;
		}
		void erase_node(node_id n_id);
		[[nodiscard]] auto get_node(node_id n_id) const noexcept -> node*;

		// 3.6.4
		void connect(node_id src, node_id dst, int slot) const;
		void disconnect(node_id src, node_id dst) const;
		[[nodiscard]] auto get_dependencies(node_id src) const -> const std::vector<std::pair<node_id, int>>;

		// 3.6.5
		[[nodiscard]] auto is_valid() const noexcept -> bool;
		[[nodiscard]] auto step() const noexcept -> bool;
		void run() const noexcept;

		// 3.6.6
		friend std::ostream &operator<<(std::ostream &, const pipeline &);


	 private:
		std::map<node_id, node*> nodes_;
		node_id current_id;
    };

}

#endif  // COMP6771_PIPELINE_H
