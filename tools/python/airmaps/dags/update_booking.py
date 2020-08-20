import logging
import os
from datetime import timedelta

from airflow import DAG
from airflow.operators.python_operator import PythonOperator
from airflow.utils.dates import days_ago

from airmaps.instruments import settings
from airmaps.instruments import storage
from booking.download_hotels import download

logger = logging.getLogger("airmaps")


DAG = DAG(
    "Update_booking",
    schedule_interval=timedelta(days=1),
    default_args={
        "owner": "MAPS.ME",
        "depends_on_past": True,
        "start_date": days_ago(0),
        "email": settings.EMAILS,
        "email_on_failure": True,
        "email_on_retry": False,
        "retries": 5,
        "retry_delay": timedelta(minutes=5),
        "priority_weight": 1,
    },
)


BOOKING_STORAGE_PATH = f"{settings.STORAGE_PREFIX}/booking/"

BOOKING_LOCAL_DIR = os.path.join(settings.MAIN_OUT_PATH, "booking")


def download_booking(**kwargs):
    booking_version = ""
    kwargs["ti"].xcom_push(key="booking_version", value=booking_version)

    os.makedirs(BOOKING_LOCAL_DIR, exist_ok=True)
    booking_file = os.path.join(BOOKING_LOCAL_DIR, booking_version)
    download(
        settings.BOOKING_USER, settings.BOOKING_PASSWORD, booking_file, threads_count=4
    )


def publish_booking(**kwargs):
    booking_version = kwargs["ti"].xcom_pull(key="booking_version")
    booking_file = os.path.join(BOOKING_LOCAL_DIR, booking_version)
    storage.wd_publish(booking_file, BOOKING_STORAGE_PATH)


def rm_booking_file(**kwargs):
    booking_version = kwargs["ti"].xcom_pull(key="booking_version")
    booking_file = os.path.join(BOOKING_LOCAL_DIR, booking_version)
    os.remove(booking_file)


DOWNLOAD_BOOKING_TASK = PythonOperator(
    task_id="Download_booking_task",
    provide_context=True,
    python_callable=download_booking,
    dag=DAG,
)


PUBLISH_BOOKING_TASK = PythonOperator(
    task_id="Publish_booking_task",
    provide_context=True,
    python_callable=publish_booking,
    dag=DAG,
)

RM_BOOKING_FILE_TASK = PythonOperator(
    task_id="Rm_booking_dir_task",
    provide_context=True,
    python_callable=rm_booking_file,
    dag=DAG,
)


DOWNLOAD_BOOKING_TASK >> PUBLISH_BOOKING_TASK >> RM_BOOKING_FILE_TASK
