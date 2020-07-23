#!/usr/bin/env python
import pika
import sys
import json
import re
connection = pika.BlockingConnection(pika.ConnectionParameters('localhost'))
channel = connection.channel()
channel.queue_declare(queue='test')
reply_queue = channel.queue_declare(queue='',exclusive=True)
msg = ""
esc_newlines_in_quotes = re.compile(r"(\"[^\"\n]*)\r?\n(?!(([^\"]*\"){2})*[^\"]*$)")
remove_trailing_commas = re.compile(",(\s*[\]}])")

def on_response(ch,method,props,body):
    global msg
    msg = body.decode("utf-8")
    print(msg)
    channel.stop_consuming()

channel.basic_publish(exchange='',routing_key='test',properties=pika.BasicProperties(reply_to=reply_queue.method.queue),body=sys.argv[1])
stop = False

channel.basic_consume(queue=reply_queue.method.queue,auto_ack=True,on_message_callback=on_response)
channel.start_consuming()
connection.close()
