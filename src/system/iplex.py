""" iplex

        Usage:
        iplex (add | delete) <port-id> [--loglevel=<value>] [--debug]
        iplex show (all | <port-id>) [--loglevel=<value>] [--debug]
        iplex -h | --help
        iplex --version

        Options:
                -h, --help      show this help message and exit
                --version       show version and exit
                --debug         show debugging output
        --loglevel=<value>  Default logvalue [default: 0]
"""

from __future__ import print_function
from docopt import docopt
import os
import errno
import pprint
import json
import logging
import sys
import time
from logging.handlers import RotatingFileHandler

LOG_FORMAT = '%(asctime)s %(name)-12s %(levelname)-8s %(message)s'
LOG_DATE = '%m-%d-%Y %H:%M:%S'
LOG_DIR = '/var/log/vfd'

VFD_CONFIG = '/etc/vfd/vfd.cfg'


def setup_logging(args, filename='iplex.log'):
    level = logging.INFO
    if args['--debug']:
        level = logging.DEBUG
    logging.basicConfig(level=level, format=LOG_FORMAT, datefmt=LOG_DATE)
    handler = RotatingFileHandler(os.path.join(LOG_DIR, filename), maxBytes=200000, backupCount=20)
    log_formatter = logging.Formatter('%(name)s: %(levelname)s %(message)s')
    handler.setFormatter(log_formatter)
    log = logging.getLogger('iplex')
    log.addHandler(handler)
    return log

def read_config():
	with open(VFD_CONFIG) as data_file:
		data = json.load(data_file)
	return data

class Iplex(object):

	PRIVATE_FIFO_PATH = "/tmp/IPLEX_"

	def __init__(self, config_data=None, options=None, log=None):
		self.options = options
		self.config_data = config_data
		self.log = log

	def add(self, port_id):
		self.filename = self.__validate_file(port_id)
		self.resp_fifo = self.__create_fifo()
		msg = self.__request_message('add', self.filename, self.resp_fifo)
		self.__write_read_fifo(msg)
		return

	def delete(self, port_id):
		self.filename = self.__validate_file(port_id)
		self.resp_fifo = self.__create_fifo()
		msg = self.__request_message('delete', self.filename, self.resp_fifo)
		self.__write_read_fifo(msg)
		return

	def show(self, port_id=None, all_list=False):
		if all_list:
			self.filename = None
		else:
			self.filename = self.__validate_file(port_id)
		self.resp_fifo = self.__create_fifo()
		msg = self.__request_message('show', self.filename, self.resp_fifo)
		self.__write_read_fifo(msg)
		return

	def __validate_file(self, port_id):
		if os.path.isfile(os.path.join(self.config_data['config_dir'], self.options['<port-id>'])+'.json'):
			filename = os.path.join(self.config_data['config_dir'], self.options['<port-id>'])+'.json'
			self.log.debug("VF Config: %s", filename)
			return filename
		else:
			self.log.info("File doesn't exist %s", filename)
			return

	def __create_fifo(self):
		resp_fifo = Iplex.PRIVATE_FIFO_PATH+str(os.getpid())
		try:
			os.mkfifo(resp_fifo)
		except OSError, e:
			self.log.info("Failed to create FIFO: %s", e)
		self.log.debug("Successfully created FIFO: %s", resp_fifo)
		return resp_fifo

	def __request_message(self, action, filename, resp_fifo):
		msg = {}
		msg['action'] = action
		msg['params'] = {}
		if filename is not None:
			msg['params']['filename'] = filename
		msg['params']['loglevel'] = self.options['--loglevel']
		msg['params']['resp_fifo'] = resp_fifo
		self.log.debug("REQUEST MESSAGE: %s", msg)
		return msg

	def __read_fd(self, fd, chunksize=1024):
		buffer = []
		running = True
		while running:
			chunk = os.read(fd, chunksize)
			buffer.append(chunk)
			running = len(chunk) == chunksize
		return ''.join(buffer).strip(' \n\t')

	def __write_read_fifo(self, msg):
		readFd = None
		try:
			writeFd = os.open(self.config_data['fifo'], os.O_WRONLY | os.O_NONBLOCK)
			os.write(writeFd, str(msg)+'\n\n')
			os.close(writeFd)
			readFd = os.open(self.resp_fifo, os.O_RDONLY)
			buf = self.__read_fd(readFd)
			resp_data = json.dumps(buf.strip(' \n\t'), sort_keys=True, indent=4)
			self.log.info("RESPONSE MSG: %s", resp_data)
			os.close(readFd)
			os.unlink(self.resp_fifo)
		except OSError as e:
			if errno.ENXIO:
				self.log.info("VF-DAEMON does not seem to be running, please start the service")
			else:
				self.log.info("%s", e)
			if not (readFd == None):
				os.close(readFd)
			os.unlink(self.resp_fifo)


if __name__ == '__main__':
	options = docopt(__doc__, version='1.0')
	log = setup_logging(options)
	log.debug("cli info : %s", options)
	config_data = read_config()
	log.debug("VFD config: %s", config_data)
	iplex = Iplex(config_data, options, log)
	if options['add']:
		iplex.add(options['<port-id>'])
	elif options['delete']:
		iplex.delete(options['<port-id>'])
	else:
		if options['show']:
			iplex.show(options['<port-id>'], options['all'])
