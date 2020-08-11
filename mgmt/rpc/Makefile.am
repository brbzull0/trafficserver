#
# Makefile.am for the Enterprise Management module.
#
#  Licensed to the Apache Software Foundation (ASF) under one
#  or more contributor license agreements.  See the NOTICE file
#  distributed with this work for additional information
#  regarding copyright ownership.  The ASF licenses this file
#  to you under the Apache License, Version 2.0 (the
#  "License"); you may not use this file except in compliance
#  with the License.  You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.

#SUBDIRS = handlers

AM_CPPFLAGS += \
  $(iocore_include_dirs) \
  -I$(abs_top_srcdir)/iocore/utils \
  -I$(abs_top_srcdir)/include \
  -I$(abs_top_srcdir)/lib/ \
  -I$(abs_top_srcdir)/mgmt/rpc \
  -I$(abs_top_srcdir)/mgmt/ \
	$(TS_INCLUDES) \
	@YAMLCPP_INCLUDES@


noinst_LTLIBRARIES = libjsonrpc_protocol.la libjsonrpc_server.la
check_PROGRAMS = test_jsonrpc


TESTS = $(check_PROGRAMS)

# Protocol library only, no transport.
libjsonrpc_protocol_COMMON = \
	jsonrpc/error/RpcError.cc \
	jsonrpc/error/RpcError.h \
	jsonrpc/JsonRpc.cc \
	jsonrpc/JsonRpc.h

libjsonrpc_protocol_la_SOURCES = \
	$(libjsonrpc_protocol_COMMON)

# RPC server only.
libjsonrpc_server_COMMON = \
	server/RpcServer.cc \
	server/RpcServer.h \
	server/TransportBase.cc \
	server/TransportBase.h \
	server/LocalUnixSocket.cc \
	server/LocalUnixSocket.h

libjsonrpc_server_la_SOURCES = \
	$(libjsonrpc_server_COMMON)

test_jsonrpc_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	-I$(abs_top_srcdir)/tests/include \
	@YAMLCPP_INCLUDES@

test_jsonrpc_LDFLAGS = \
	@AM_LDFLAGS@

test_jsonrpc_SOURCES = \
    unit_tests/unit_test_main.cc \
	unit_tests/test_basic_protocol.cc \
	unit_tests/test_rpcserver.cc


test_jsonrpc_LDADD = \
	libjsonrpc_protocol.la \
	libjsonrpc_server.la \
	$(top_builddir)/src/tscpp/util/libtscpputil.la \
	$(top_builddir)/lib/records/librecords_p.a \
	$(top_builddir)/src/tscore/libtscore.la \
	$(top_builddir)/iocore/eventsystem/libinkevent.a \
	$(top_builddir)/lib/records/librecords_p.a \
	$(top_builddir)/iocore/eventsystem/libinkevent.a \
	$(top_builddir)/src/tscore/libtscore.la \
	$(top_builddir)/mgmt/libmgmt_p.la \
	$(top_builddir)/proxy/shared/libUglyLogStubs.a \
	@YAMLCPP_LIBS@
#may need to use the default one at @YAML@
#	$(top_builddir)/mgmt/rpc/handlers/librpcpublichandlers.la

include $(top_srcdir)/build/tidy.mk

clang-tidy-local: $(DIST_SOURCES)
	$(CXX_Clang_Tidy)

