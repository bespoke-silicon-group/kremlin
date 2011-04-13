#!/usr/bin/env python

import httplib
import urllib
import string
import optparse
import logging
import copy
import re
import xml.dom.minidom
from getpass import getpass

# Set up a specific logger with our desired output level
log = logging.getLogger(__name__)
log.setLevel(logging.DEBUG)

# Add the log message handler to the logger
log.addHandler(logging.StreamHandler())

class HttpsConnection:
    def __init__(self, url):
        self.conn = httplib.HTTPSConnection(url)

    def get(self, url, body, headers):
        return self.request("GET", url, body, headers)

    def log_response(self, response, body):
        log.debug("\n".join(
            ["Response",
                str(response.status) + " " + str(response.reason),
                body]))

    def log_request(self, method, url, body, headers):
        if string.find(body, "Passwd") != -1:
            body = "[body hidden]"

        log.debug("\n".join(
            [str(method) + " " + str(url),
                str(headers),
                str(body)]))

    def request(self, method, url, body, headers):
        self.log_request(method, url, body, headers)

        self.conn.request(method, url, body, headers)

        response = self.conn.getresponse()
        response_body = response.read()

        self.log_response(response, response_body)

        return (response, response_body)

    def post(self, url, body, headers):
        return self.request("POST", url, body, headers)

    def close(self):
        self.conn.close()

class Authenticator:
    def __init__(self):
        self.auth = None

    def authenticate(self):
        url = "www.google.com"
        conn = HttpsConnection(url)
        client_login_url = "/accounts/ClientLogin"
        client_login_headers = {"Content-type" : "application/x-www-form-urlencoded"}

        email = "ChristopherLouie88@gmail.com"
        password = getpass("Password for " + email + ": ")

        client_login_params = urllib.urlencode({
            "accountType" : "GOOGLE",
            "Email" : email,
            "Passwd" : password,
            "service" : "writely",
            "source" : "kremlin-kremlin-0.1"})

        log.debug("authenticating...")
        client_login_response, client_login_response_body = conn.post(
            client_login_url, client_login_params, 
            client_login_headers)

        if client_login_response.status == httplib.FORBIDDEN:
            raise "Failed to authenticate " + email + ": " + client_login_response.reason

        for line in client_login_response_body.splitlines():
            if "Auth=" in line:
                self.auth = line[5:]

        conn.close()
        return self.auth

    def get_auth(self):
        return self.auth

class GDocs:
    def __init__(self, auth):
        url = "docs.google.com"
        self.auth = auth
        self.conn = HttpsConnection(url)

    def close(self):
        self.conn.close()

    def get_base_header(self):
        return {
            "GData-Version" : "3.0",
            "Authorization" : "GoogleLogin auth=" + auth}

    def upload_spreadsheet(self, mime_type, folder, title, data):

        if folder != None:
            folder_uri = "/folder%3A" + folder.id + "/contents"
        else:
            folder_uri = ""

        uri = "/feeds/default/private/full" + folder_uri
        headers = self.get_base_header()
        headers["Content-Type"] = mime_type
        headers["Slug"] = title
        body = data

        log.debug("Uploading to GDocs.")
        response, response_body = self.conn.post(uri, body, headers)

    def query(self, query, type = None):

        if type == None: 
            type_query = "" 
        else: 
            type_query = "-/" + type

        uri = "/feeds/default/private/full/" + type_query + "?" + urllib.urlencode({
            "q" : query })

        headers = self.get_base_header()
        response, response_body = self.conn.get(uri, "", headers)
        return response_body

    def get_doc(self, query, type = None):
        body = self.query(query, type)

        id_matcher = re.compile(r"<gd:resourceId>([^<]*)</gd:resourceId>")
        match = id_matcher.search(body)

        return Document(match.group(1))

class Document:
    def __init__(self, id_str):
        tokens = id_str.split(":")

        self.type = tokens[0]
        self.id = tokens[1]

    def __repr__(self):
       return self.type + ":" + self.id

if __name__ == "__main__":

    log.info("Upload to google docs")

    auth = Authenticator().authenticate()

    gdocs = GDocs(auth)
    folder = gdocs.get_doc("Regression", "folder")
    gdocs.upload_spreadsheet("text/CSV", folder, "test spreadsheet", "a1,b1\na2,b2")

