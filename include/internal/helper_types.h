#pragma once

namespace knet
{
	namespace internal
	{
		inline namespace helpers
		{
			class non_copyable
			{
			protected:
				non_copyable() = default;
				virtual ~non_copyable() = default;

				non_copyable(non_copyable const &) = delete;
				void operator=(non_copyable const &x) = delete;
			};
		}
	};
};