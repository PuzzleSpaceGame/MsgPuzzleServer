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

def on_response(ch,method,props,body):
    global msg
    msg = body.decode("utf-8")
    channel.stop_consuming()

outgoing = "GETCFG"
channel.basic_publish(exchange='',routing_key='test',properties=pika.BasicProperties(reply_to=reply_queue.method.queue),body=outgoing)
channel.basic_consume(queue=reply_queue.method.queue,auto_ack=True,on_message_callback=on_response)
channel.start_consuming()
parsed_msg = json.loads(msg)


outgoing = "NEWXXX-6,6,1,0,1,2,2"
channel.basic_publish(exchange='',routing_key='test',properties=pika.BasicProperties(reply_to=reply_queue.method.queue),body=outgoing)
channel.basic_consume(queue=reply_queue.method.queue,auto_ack=True,on_message_callback=on_response)
channel.start_consuming()
parsed_msg = json.loads(msg)
gamestate = parsed_msg["gamestate"]

outgoing = f"UPDATE-{gamestate}-3,3,0"
channel.basic_publish(exchange='',routing_key='test',properties=pika.BasicProperties(reply_to=reply_queue.method.queue),body=outgoing)
channel.basic_consume(queue=reply_queue.method.queue,auto_ack=True,on_message_callback=on_response)
channel.start_consuming()
parsed_msg = json.loads(msg)
gamestate = parsed_msg["gamestate"]


outgoing = f"UPDATE-{gamestate}-3,3,0"
channel.basic_publish(exchange='',routing_key='test',properties=pika.BasicProperties(reply_to=reply_queue.method.queue),body=outgoing)
channel.basic_consume(queue=reply_queue.method.queue,auto_ack=True,on_message_callback=on_response)
channel.start_consuming()
parsed_msg = json.loads(msg)
gamestate = parsed_msg["gamestate"]

outgoing = f"REDRAW-{gamestate}"
channel.basic_publish(exchange='',routing_key='test',properties=pika.BasicProperties(reply_to=reply_queue.method.queue),body=outgoing)
channel.basic_consume(queue=reply_queue.method.queue,auto_ack=True,on_message_callback=on_response)
channel.start_consuming()
parsed_msg = json.loads(msg)
gamestate = parsed_msg["gamestate"]

outgoing = "KILLXX"
channel.basic_publish(exchange='',routing_key='test',properties=pika.BasicProperties(reply_to=reply_queue.method.queue),body=outgoing)
channel.basic_consume(queue=reply_queue.method.queue,auto_ack=True,on_message_callback=on_response)
channel.start_consuming()
connection.close()
