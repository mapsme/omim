require 'twine_test_case'

class TestStringsRow < TwineTestCase
  def setup
    super

    @reference = Twine::StringsRow.new 'reference-key'
    @reference.comment = 'reference comment'
    @reference.tags = ['ref1']
    @reference.translations['en'] = 'ref-value'

    @row = Twine::StringsRow.new 'key'
    @row.reference_key = @reference.key
    @row.reference = @reference
  end

  def test_reference_comment_used
    assert_equal 'reference comment', @row.comment
  end

  def test_reference_comment_override
    @row.comment = 'row comment'

    assert_equal 'row comment', @row.comment
  end

  def test_reference_tags_used
    assert @row.matches_tags?(['ref1'], false)
  end

  def test_reference_tags_override
    @row.tags = ['tag1']

    refute @row.matches_tags?(['ref1'], false)
    assert @row.matches_tags?(['tag1'], false)
  end

  def test_reference_translation_used
    assert_equal 'ref-value', @row.translated_string_for_lang('en')
  end

  def test_reference_translation_override
    @row.translations['en'] = 'value'

    assert_equal 'value', @row.translated_string_for_lang('en')
  end
end
