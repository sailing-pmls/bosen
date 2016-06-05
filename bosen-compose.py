#!/usr/bin/env python

import argparse
import os
import string
import sys

constant_args = {
    'clients_file': 'clients.txt',
    'input_file': 'input.txt',
    'output_dir': 'output',
    'client_prefix': 'c',
    'compose_file': 'docker-compose.yml'
}

header_template = """version: '2'
services:
"""

client_template = string.Template("""
        ${client_prefix}${client_id}:
                image: petuum/bosen
                volumes:
                 - ${path}:/data
                command: >
                       --num_worker_threads=4
                       --hostfile=/data/${clients_file}
                       --client_id=${client_id}
                       --num_clients=${num_clients}
                       --init_R_low=0.5
                       --load_cache=0
                       --init_step_size=0.05
                       --rank=3
                       --init_R_high=1.0
                       --num_iter_L_per_minibatch=10
                       --num_epochs=800
                       --minibatch_size=9
                       --step_size_offset_R=0.0
                       --output_data_format=text
                       --step_size_offset_L=0.0
                       --num_eval_samples=100
                       --num_eval_minibatch=10
                       --table_staleness=0
                       --m=9
                       --is_partitioned=0
                       --step_size_pow_R=0.0
                       --cache_path=/data/cache
                       --step_size_pow_L=0.0
                       --maximum_running_time=0.0
                       --step_size_offset=0.0
                       --step_size_pow=0.0
                       --init_L_low=0.5
                       --init_step_size_L=0.0
                       --input_data_format=text
                       --n=9 --data_file=/data/${input_file}
                       --init_step_size_R=0.0
                       --output_path=/data/${output_dir}
                       --init_L_high=1.0
""")

def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("path", help="Path to application data files", type=str)
    parser.add_argument("num_clients", help="Number of clients", type=int)
    return parser.parse_args()

def generate_all_args(args):
    all_args = constant_args.copy()
    all_args['path'] = args.path
    all_args['num_clients'] = args.num_clients
    return all_args

def generate_clients_file(args):
    clients_file = os.path.join(args['path'], args['clients_file'])
    basename = os.path.basename(args['path'])
    with open(clients_file, "w") as f:
        for i in range(args['num_clients']):
            f.write("%d %s_%s%d_1 9999\n" %
                    (i, basename, args['client_prefix'], i))

def generate_compose_file(args):
    compose_file = os.path.join(args['path'], args['compose_file'])
    with open(compose_file, "w") as f:
        f.write(header_template)
        for i in range(args['num_clients']):
            f.write(client_template.substitute(args, client_id=i))

def main():
    all_args = generate_all_args(parse_args())

    try:
        os.mkdir(all_args['path'])
        os.mkdir(os.path.join(all_args['path'], all_args['output_dir']))
    except Exception, err:
        sys.stderr.write("Failed to create directory '%s': %s\n"
                         % (all_args['path'], str(err)))
        return 1

    generate_clients_file(all_args)
    generate_compose_file(all_args)

    return 0

if __name__ == "__main__":
    sys.exit(main())
