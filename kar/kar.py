#!/usr/bin/python3

from tensorflow.keras.models import load_model
from collections import defaultdict
from sklearn import metrics
from sklearn.externals import joblib
import tensorflow.keras
import tensorflow as tf
import numpy as np
import argparse
import logging
import pyodbc
import json
import os

logging.basicConfig(level=logging.INFO,
                    format='[%(asctime)s] [%(levelname)s] %(message)s', datefmt='%Y-%m-%d %H:%M:%S')

parser = argparse.ArgumentParser()
parser.add_argument("-p", "--password",
                    help="File containing connection information", default=os.path.dirname(__file__) + "/passwd.json")
parser.add_argument("-r", "--run_id",
                    help="The id to evaluate", default=1880, type=int)
parser.add_argument("-o", "--output", help="Json file for metrics")
parser.add_argument("-k", "--keras", help="Keras H5 NN file",
                    default=os.path.dirname(__file__) + "/DNN.h5")
parser.add_argument("-s", "--sklearn", help="sklearn model file",
                    default=os.path.dirname(__file__) + "/DNN.joblib")
credentials = dict()


def loadPassword(file):
    with open(file, "r") as f:
        jData = json.load(f)
        credentials["Username"] = jData["Username"]
        credentials["Password"] = jData["Password"]
        credentials["server"] = jData["server"]
        credentials["Database"] = jData["Database"]


def main(args):
    if not os.path.isfile(args.password):
        logging.critical(f"Could not locate password file at {args.password}")
        return
    loadPassword(args.password)
    logging.info("Loading models")
    if not os.path.isfile(args.keras):
        logging.critical(f"Could not locate Keras file at {args.keras}")
        return
    pers = joblib.load(args.sklearn)
    if not os.path.isfile(args.sklearn):
        logging.critical(f"Could not locate sklearn file at {args.sklearn}")
        return
    model = load_model(args.keras)
    mlb = pers["Binarizer"]
    logging.info("Getting data")
    db = GetDataLabels(args.run_id)
    logging.info("Preprocessing data")
    data = pers["Pipeline"].transform(db[0])
    logging.info("Executing DNN")
    y_prob = model.predict(data)
    pred = mlb.classes_[np.argmax(y_prob, axis=1)]
    if args.output != None:
        res = metrics.classification_report(
            db[1], pred, output_dict=True, zero_division=0)
        with open(args.output, 'w') as fp:
            json.dump(res, fp)
    else:
        res = metrics.classification_report(
            db[1], pred, output_dict=False, zero_division=0)
        print(res)
    logging.info("Complete")


def GetLabels(runId):
    cnxn = pyodbc.connect('DRIVER={ODBC Driver 17 for SQL Server};SERVER=' +
                          credentials["server"]+';DATABASE='+credentials["Database"]+';UID='+credentials["Username"]+';PWD=' + credentials["Password"])
    cursor = cnxn.cursor()

    result = defaultdict(str)

    cursor.execute("select UID, Labels, RunId from Kernels WHERE RunId = " +
                   str(runId) + " and Labels IS NOT NULL ORDER BY UID")
    rows = cursor.fetchall()
    for row in rows:
        uid = row[0]
        label = row[1]
        if label == None:
            label = ""
        result[uid] = label.split(";")
    return result


def GetData(runId):
    cnxn = pyodbc.connect('DRIVER={ODBC Driver 17 for SQL Server};SERVER=' +
                          credentials["server"]+';DATABASE='+credentials["Database"]+';UID='+credentials["Username"]+';PWD=' + credentials["Password"])
    cursor = cnxn.cursor()

    result = defaultdict(lambda: defaultdict(lambda: -1.0))

    cursor.execute("select * from Kernels INNER JOIN pig ON Kernels.PigId = pig.UID AND Kernels.RunId = " +
                   str(runId) + " and Labels IS NOT NULL ORDER BY Kernels.UID")
    rows = cursor.fetchall()
    columns = [column[0] for column in cursor.description]
    for row in rows:
        uid = row[0]
        count = float(row[25])  # column with the instruction count
        for i in range(19, 90):
            if i != 25:
                result[uid]["Pig:" + columns[i]] = float(row[i]) / count

    cursor.execute("select * from Kernels INNER JOIN cpig ON Kernels.CpigId = cpig.UID AND Kernels.RunId = " +
                   str(runId) + " and Labels IS NOT NULL ORDER BY Kernels.UID")
    rows = cursor.fetchall()
    columns = [column[0] for column in cursor.description]
    for row in rows:
        uid = row[0]
        count = float(row[25])  # column with the instruction count
        for i in range(19, 90):
            if i != 25:
                result[uid]["CPig:" + columns[i]] = float(row[i]) / count

    cursor.execute("select * from Kernels INNER JOIN epig ON Kernels.EPigId = epig.UID AND Kernels.RunId = " +
                   str(runId) + " and Labels IS NOT NULL ORDER BY Kernels.UID")
    rows = cursor.fetchall()
    columns = [column[0] for column in cursor.description]
    for row in rows:
        uid = row[0]
        # column with the instruction count #we don't know which index is the total
        count = float(row[409])
        for i in range(19, 410):
            if i != 409:  # we don't know which index is the total
                result[uid]["EPig:" + columns[i]] = float(row[i]) / count

    cursor.execute("select * from Kernels INNER JOIN ecpig ON Kernels.ECPigId = ecpig.UID AND Kernels.RunId = " +
                   str(runId) + " and Labels IS NOT NULL ORDER BY Kernels.UID")
    rows = cursor.fetchall()
    columns = [column[0] for column in cursor.description]
    for row in rows:
        uid = row[0]
        # column with the instruction count #we don't know which index is the total
        count = float(row[409])
        for i in range(19, 410):
            if i != 409:  # we don't know which index is the total
                result[uid]["ECPig:" + columns[i]] = float(row[i]) / count
    return result


def GetDataLabels(runId):
    result = []
    data = GetData(runId)
    labels = GetLabels(runId)
    uids = []

    values = []
    finalLabels = []
    # start by getting all the uids
    keys = set()
    uidset = set()
    for uid in data:
        uidset.add(uid)
        for key in data[uid]:
            keys.add(key)
    names = []
    for key in keys:
        names.append(key)
    names.sort()
    # then
    suids = []
    for uid in uidset:
        suids.append(uid)
    suids.sort()
    for uid in suids:
        tempVal = []
        finLab = ""
        for key in names:
            tempVal.append(data[uid][key])
        for lab in labels[uid]:
            spl = lab.split("[")
            l = spl[0]
            finLab = l
        values.append(np.array(tempVal))
        finalLabels.append(finLab)
        uids.append(uid)
    result.append(np.array(values))
    result.append(np.array(finalLabels))
    result.append(np.array(uids))
    result.append(np.array(names))
    return result


if __name__ == "__main__":
    args = parser.parse_args()
    os.environ['TF_CPP_MIN_LOG_LEVEL'] = '3'
    main(args)
