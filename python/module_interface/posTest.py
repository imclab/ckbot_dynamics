import time, json, sys
from math import pi
from random import random
from struct import pack, unpack

import moduleIface

class PositionLogger(object):
    """
    Class that listens to a serial line on which
    parkourbot modules are reporting their positions.
    This parses that data and stores it as json along with
    as much information about the test as possible.
    """ 

    def __init__(self,
                 modIface=None,   # The module interface on which we communicate
                 test_time=None, # Len in [s] of trajectory
                 jsonout=dict(), # Json dictionary to store results and stup
                 test_name="PositionLogger_" + str(int(time.time())),
                 debug=False     # Use debug output?
                 ):
        """
        dev     - device string of serial device associated
                  with the module network.
        modules - list of module IDs in the chain (Brain board IDs)
        debug   - Boolean indicating whether debug output should be printed
        jsonout - dictionary to use as the json output object for this test.
                  This could be used to write the result and setup of this
                  test to file, so fill it with all of the information needed
                  to recreate it.
        """
        self.debug = debug

        if not modIface:
            raise Exception("You need to supply a moduleIface class instance")
        self.m = modIface

        self.jsonout = jsonout
        if (not self.jsonout.get('results')):
            self.jsonout.setdefault('results', dict())

        if not test_name:
            test_name = "PositionLogger_" + str(int(time.time()))
        self.test_name = test_name

        if not test_time:
            raise Exception("PositionLogger: Must specify a test length with test_time paramter")
        self.test_time = test_time

        self.modules = dict()
        self.converted_data = list()
        self.data = list()

    def debugOut(self, msg):
        if self.debug: print msg

    def run_test(self):
        # TODO (IMO): Curse you Uriah!  What the hell are the stop/start
        #             packet structures?
        pkt = '000'
        stop = 'f' + pack('B', len(pkt)).encode('hex').upper() + '100' + '\r'
        start = 'f' + pack('B', len(pkt)).encode('hex').upper() + '0' + pack('B',
                self.test_time).encode('hex').upper() + '\r'

        print "Running Test: %s" % self.test_name
        print "Test time is: %f" % self.test_time

        self.m.ser.write(start)

        time.sleep(self.test_time+1)

        msg = ''
        pkts = list()
        while True:
            b = self.m.ser.read()
            if b == '':
                break
            msg += b
            if b == '\r':
                pkts.append(msg)
                msg = ''
        for pkt in pkts:
            if len(pkt) != 10:
                self.debugOut("  Found a faulty packet: %s" % repr(pkt))
                continue
            # TODO(IMO): What's the packet format?
            vals = unpack('<BHHB', pkt[3:-1])
            self.debugOut("  Read packet: %s" % repr(vals))
            self.data.append(vals)
            m_id = vals[0]
            self.modules[m_id] = m_id
            pos = vals[1]
            if pos > 2**15:
                self.debugOut("  ID: %d, Pos greater than 2**15" % m_id)
                continue
            pos_raw = pos
            if pos_raw > 2**14:
                pos_raw = pos_raw - 2**15
            position = pi*(pos_raw/float(2**15))
            pkt_time = float(vals[2])/1000.0
            voltage = float(vals[3])/2**5

            converted_data = (m_id, position, pkt_time, voltage)
            self.converted_data.append(converted_data)
            self.debugOut("  Successful Packet with ID:%d, pos [rad]: %f, Time: %f, Voltage: %f" % converted_data)

        time_str = time.strftime("%a, %d %b %Y %H:%M:%S +0000", time.localtime())

        self.jsonout['results']['date'] = time_str
        self.jsonout['results']['name'] = self.test_name
        self.jsonout['results']['time'] = self.test_time
        self.jsonout['results']['data'] = self.data
        self.jsonout['results']['converted_data'] = self.converted_data
        self.jsonout['results']['modules'] = self.modules

        print "Saving test data to: %s" % self.test_name
        with open(self.test_name, 'w') as f:
            json.dump(self.jsonout, f)
        print "Test successfully completed"

