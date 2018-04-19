import unittest
import datetime
import pykmlib

class PyKmlibAdsTest(unittest.TestCase):

    def test_smoke(self):
        category = pykmlib.CategoryData()
        category.name.set_default('Test category')
        category.name['ru'] = 'Тестовая категория'
        category.description.set_default('Test description')
        category.description['ru'] = 'Тестовое описание'
        category.annotation.set_default('Test annotation')
        category.annotation['en'] = 'Test annotation'
        category.image_url = 'https://localhost/123.png'
        category.visible = True
        category.author_name = 'Maps.Me'
        category.author_id = '12345'
        category.rating = 8.9
        category.reviews_number = 567
        category.last_modified = int(datetime.datetime.now().timestamp())
        category.access_rules = pykmlib.AccessRules.PUBLIC
        category.tags.set_list(['mountains', 'ski', 'snowboard'])
        category.cities.set_list([pykmlib.LatLon(35.2424, 56.2164), pykmlib.LatLon(34.2443, 46.3536)])
        category.languages.set_list(['en', 'ru', 'de'])
        category.properties.set_dict({'property1':'value1', 'property2':'value2'})

        bookmark = pykmlib.BookmarkData()
        bookmark.name.set_default('Test bookmark')
        bookmark.name['ru'] = 'Тестовая метка'
        bookmark.description.set_default('Test bookmark description')
        bookmark.description['ru'] = 'Тестовое описание метки'
        bookmark.feature_types.set_list([8, 13, 34, 565])
        bookmark.custom_name.set_default('Мое любимое место')
        bookmark.custom_name['en'] = 'My favorite place'
        bookmark.color.predefined_color = pykmlib.PredefinedColor.BLUE
        bookmark.color.rgba = 0
        bookmark.icon = pykmlib.BookmarkIcon.NONE
        bookmark.viewport_scale = 15
        bookmark.timestamp = int(datetime.datetime.now().timestamp())
        bookmark.point = pykmlib.LatLon(45.9242, 56.8679) 
        bookmark.bound_tracks.set_list([0])

        layer1 = pykmlib.TrackLayer()
        layer1.line_width = 6.0
        layer1.color.rgba = 0xff0000ff
        layer2 = pykmlib.TrackLayer()
        layer2.line_width = 7.0
        layer2.color.rgba = 0x00ff00ff

        track = pykmlib.TrackData()
        track.local_id = 1
        track.name.set_default('Test track')
        track.name['ru'] = 'Тестовый трек'
        track.description.set_default('Test track description')
        track.description['ru'] = 'Тестовое описание трека'
        track.timestamp = int(datetime.datetime.now().timestamp())
        track.layers.set_list([layer1, layer2])
        track.points.set_list([
        	pykmlib.LatLon(45.9242, 56.8679),
        	pykmlib.LatLon(45.2244, 56.2786),
        	pykmlib.LatLon(45.1964, 56.9832)])

        file_data = pykmlib.FileData()
        file_data.category = category
        file_data.bookmarks.append(bookmark)
        file_data.tracks.append(track)

        s = pykmlib.export_kml(file_data)
        imported_file_data = pykmlib.import_kml(s)
        self.assertEqual(file_data, imported_file_data)


if __name__ == "__main__":
    unittest.main()
