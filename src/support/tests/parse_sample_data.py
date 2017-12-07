from ConfigParser import ConfigParser

# This will return sample data object
def sample_data_obj():
    data = ConfigParser()
    data.read("sample_data.ini")
    return data

# return section dict object
def section_map(section):
    dict_obj = {}
    options = sample_data_obj().options(section)
    for opt in options:
        try:
            dict_obj[opt] = sample_data_obj().get(section, opt)
        except:
            dict_obj[opt] = None
    return dict_obj

if __name__ == '__main__':
    dict_obj = section_map('TARGET_VF1')
    print [mac.strip() for mac in dict_obj['valid_vlans'].strip().split(',')]
