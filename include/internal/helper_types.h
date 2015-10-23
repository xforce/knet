// Copyright 2015 the kNet authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
